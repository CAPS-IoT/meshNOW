#include "packet_handler.hpp"

#include <esp_log.h>

#include <lock.hpp>

#include "event.hpp"
#include "fragments.hpp"
#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("PacketHandler");

void PacketHandler::handlePacket(const util::MacAddr& from, int rssi, const packets::Packet& packet) {
    // TODO handle duplicate packets
    // TODO update routing table
    // TODO detect and handle cycles

    auto payload = packet.payload;

    MetaData meta{
        .last_hop = from,
        .from = packet.from,
        .rssi = rssi,
    };

    // simply visit the corresponding overload
    Lock lock;
    std::visit([&](const auto& p) { handle(meta, p); }, payload);
}

/**
 * Helper functions
 */
namespace {

inline bool lastHopIsFrom(const MetaData& meta) { return meta.from == meta.last_hop; }

inline layout::Layout& layout() { return layout::Layout::get(); }

inline bool reachesRoot() {
    if (state::getState() == state::State::REACHES_ROOT) {
        assert((state::isRoot() || layout().hasParent()) && "By this point, must have either a parent or be the root");
        return true;
    } else {
        return false;
    }
}

inline bool knowsNode(const util::MacAddr& mac) { return layout().has(mac); }

inline bool isParent(const util::MacAddr& mac) {
    if (layout().hasParent()) {
        assert(state::getState() != state::State::DISCONNECTED_FROM_PARENT &&
               "Must not be disconnected from parent if there is a parent");
        return layout().getParent().mac == mac;
    }
    return false;
}

inline bool isChild(const util::MacAddr& mac) {
    auto children = layout().getChildren();
    return std::any_of(children.begin(), children.end(), [&](const layout::Child& child) { return child.mac == mac; });
}

inline bool isNeighbor(const util::MacAddr& mac) { return isParent(mac) || isChild(mac); }

inline bool canAcceptNewChild() { return layout().getChildren().size() < layout::MAX_CHILDREN; }

inline bool disconnected() {
    if (state::getState() == state::State::DISCONNECTED_FROM_PARENT) {
        assert(!state::isRoot() && "Cannot be disconnected and root at the same time");
        assert(!layout().hasParent() && "Cannot have a parent by this point");
        return true;
    } else {
        return false;
    }
}

}  // namespace

// HANDLERS //

void PacketHandler::handle(const MetaData& meta, const packets::Status& p) {
    if (!lastHopIsFrom(meta)) return;

    auto& layout = layout::Layout::get();

    // is child?
    if (layout.hasChild(meta.from)) {
        auto& child = layout.getChild(meta.from);
        child.last_seen = xTaskGetTickCount();
    }

    // is parent?
    if (layout.hasParent()) {
        auto& parent = layout.getParent();
        if (parent.mac != meta.from) return;

        parent.last_seen = xTaskGetTickCount();
        //        switch (p.state) {
        //            case state::State::DISCONNECTED_FROM_PARENT:
        //            case state::State::CONNECTED_TO_PARENT: {
        //                state::setState(state::State::CONNECTED_TO_PARENT);
        //                break;
        //            }
        //            case state::State::REACHES_ROOT: {
        //                // this should never be the case, but we never know with malicious packets
        //                if (!p.root.has_value()) return;
        //
        //                // set root mac
        //                state::setRootMac(p.root.value());
        //                // set state
        //                state::setState(state::State::REACHES_ROOT);
        //                break;
        //            }
        //        }
        return;
    }
}

void PacketHandler::handle(const MetaData& meta, const packets::SearchProbe& p) {
    if (!lastHopIsFrom(meta)) return;
    if (!reachesRoot()) return;
    if (knowsNode(meta.from)) return;
    if (!canAcceptNewChild()) return;

    // send reply
    ESP_LOGV(TAG, "Sending I Am Here");
    send::enqueuePayload(packets::SearchReply{}, send::DirectOnce{meta.from});
}

void PacketHandler::handle(const MetaData& meta, const packets::SearchReply&) {
    if (!lastHopIsFrom(meta)) return;
    if (!disconnected()) return;
    if (knowsNode(meta.from)) return;

    // fire event to let connect job know
    event::ParentFoundData data{
        .parent = meta.from,
        .rssi = meta.rssi,
    };
    event::Internal::fire(event::InternalEvent::PARENT_FOUND, &data, sizeof(data));
}

void PacketHandler::handle(const MetaData& meta, const packets::ConnectRequest& p) {
    if (!lastHopIsFrom(meta)) return;
    if (!reachesRoot()) return;
    if (knowsNode(meta.from)) return;
    if (!canAcceptNewChild()) return;

    // add to layout
    layout().addChild(meta.from);

    ESP_LOGI(TAG, "Child " MACSTR " connected", MAC2STR(meta.from));

    // send reply
    ESP_LOGV(TAG, "Sending Connect Response");
    send::enqueuePayload(packets::ConnectOk{state::getRootMac()}, send::DirectOnce(meta.from));
}

void PacketHandler::handle(const MetaData& meta, const packets::ConnectOk& p) {
    if (!lastHopIsFrom(meta)) return;
    if (!disconnected()) return;
    if (knowsNode(meta.from)) return;

    // fire event to let connect job know
    event::GotConnectResponseData data{
        .parent = meta.from,
        .root = p.root,
    };
    event::Internal::fire(event::InternalEvent::GOT_CONNECT_RESPONSE, &data, sizeof(data));
}

void PacketHandler::handle(const MetaData& meta, const packets::RoutingTableAdd& p) {
    // TODO
}

void PacketHandler::handle(const MetaData& meta, const packets::RoutingTableRemove& p) {
    // TODO
}

void PacketHandler::handle(const MetaData& meta, const packets::RootUnreachable& p) {
    if (!reachesRoot()) return;
    if (!isParent(meta.last_hop)) return;

    ESP_LOGI(TAG, "Got Root Unreachable packet from parent");

    state::setState(state::State::CONNECTED_TO_PARENT);

    // TODO forward
}

void PacketHandler::handle(const MetaData& meta, const packets::RootReachable& p) {
    if (reachesRoot()) return;
    if (!isParent(meta.last_hop)) return;

    ESP_LOGI(TAG, "Got Root Reachable packet from parent");

    state::setState(state::State::REACHES_ROOT);

    // todo forward
}

void PacketHandler::handle(const MetaData& meta, const packets::DataFragment& p) {
    if (!isNeighbor(meta.last_hop)) return;

    // add to fragment reassembly
    fragments::addFragment(meta.from, p.frag_id, p.options.unpacked.frag_num, p.options.unpacked.total_size, p.data);
}

void PacketHandler::handle(const MetaData& meta, const packets::CustomData& p) {
    // TODO
}

}  // namespace meshnow::job