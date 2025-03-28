#include "packet_handler.hpp"

#include <esp_log.h>

#include <algorithm>
#include <lock.hpp>

#include "custom.hpp"
#include "event.hpp"
#include "fragments.hpp"
#include "layout.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("PacketHandler");

static bool isForMe(const packets::Packet& packet) {
  if (packet.to == state::getThisMac()) return true;
  if (packet.to == util::MacAddr::broadcast()) return true;
  if (packet.to == util::MacAddr::root() && state::isRoot()) return true;
  return false;
}

// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void PacketHandler::handlePacket(const util::MacAddr& from, int rssi,
                                 const packets::Packet& packet) {
  // TODO handle duplicate packets
  // TODO update routing table
  // TODO detect and handle cycles

  auto& payload = packet.payload;

  printf("Packet: " MACSTR " -> " MACSTR "(hop: " MACSTR ")",
         MAC2STR(packet.from), MAC2STR(packet.to), MAC2STR(from));

  std::visit(
      overloaded{
          [](const packets::Status& p) { printf(" - Status"); },
          [](const packets::SearchProbe& p) { printf(" - SearchProbe"); },
          [](const packets::SearchReply& p) { printf(" - SearchReply"); },
          [](const packets::ConnectRequest& p) { printf(" - ConnectRequest"); },
          [](const packets::ConnectOk& p) { printf(" - ConnectOk\n"); },
          [](const packets::RoutingTableAdd& p) {
            printf(" - RoutingTableAdd");
          },
          [](const packets::RoutingTableRemove& p) {
            printf(" - RoutingTableRemove");
          },
          [](const packets::RootUnreachable& p) {
            printf(" - RootUnreachable");
          },
          [](const packets::RootReachable& p) { printf(" - RootReachable"); },
          [](const packets::DataFragment& p) { printf(" - DataFragment"); },
          [](const packets::CustomData& p) { printf(" - CustomData"); },
      },
      payload);

  if (isForMe(packet)) {
    printf(" - For me\n");
  } else {
    printf(" - Not for me\n");
  }

  // dynamiclly update routing table
  if (layout::Layout::get().hasChild(from)) {
    auto& child = layout::Layout::get().getChild(from);
    for (const auto& entry : child.routing_table) {
      if (entry.mac == packet.from) goto routing_table_done;
    }

    // child does not know sender, add to routing table
    child.routing_table.emplace_back(packet.from);

    // check if sender is part of routing table of other children
    // if so, remove from their routing table
    for (auto& other_child : layout::Layout::get().getChildren()) {
      if (other_child.mac == from) continue;

      auto& routing_table = other_child.routing_table;
      routing_table.erase(
          std::remove_if(
              routing_table.begin(), routing_table.end(),
              [&](const auto& entry) { return entry.mac == packet.from; }),
          routing_table.end());
    }
  }

routing_table_done:

  // forward if not designated to this node
  if (!isForMe(packet)) {
    send::enqueuePayload(packet.payload,
send::FullyResolve(packet.from, packet.to, from),
                         packet.id);
    return;
  }

  // if broadcast, send to every node
  if (packet.to == util::MacAddr::broadcast()) {
    send::enqueuePayload(packet.payload,
                         send::FullyResolve(packet.from, packet.to, from),
                         packet.id);
  }

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

inline bool lastHopIsFrom(const MetaData& meta) {
  return meta.from == meta.last_hop;
}

inline layout::Layout& layout() { return layout::Layout::get(); }

inline bool reachesRoot() {
  if (state::getState() == state::State::REACHES_ROOT) {
    assert((state::isRoot() || layout().hasParent()) &&
           "By this point, must have either a parent or be the root");
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
  return std::any_of(
      children.begin(), children.end(),
      [&](const layout::Child& child) { return child.mac == mac; });
}

inline bool isNeighbor(const util::MacAddr& mac) {
  return isParent(mac) || isChild(mac);
}

inline bool canAcceptNewChild() {
  return layout().getChildren().size() < layout::MAX_CHILDREN;
}

inline bool disconnected() {
  if (state::getState() == state::State::DISCONNECTED_FROM_PARENT) {
    assert(!state::isRoot() &&
           "Cannot be disconnected and root at the same time");
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

  // printf("STATUS: from " MACSTR, MAC2STR(meta.from));
  // if (p.state == state::State::DISCONNECTED_FROM_PARENT) {
  //   printf(" - DISCONNECTED_FROM_PARENT");
  // } else if (p.state == state::State::CONNECTED_TO_PARENT) {
  //   printf(" - CONNECTED_TO_PARENT");
  // } else if (p.state == state::State::REACHES_ROOT) {
  //   printf(" - REACHES_ROOT");
  // }

  // // is child?
  if (layout.hasChild(meta.from)) {
    auto& child = layout.getChild(meta.from);
    child.last_seen = xTaskGetTickCount();
    // printf(" - is Child\n");
  }

  // is parent?
  if (layout.hasParent()) {
    auto& parent = layout.getParent();
    if (parent.mac != meta.from) return;

    // printf(" - is Parent\n");

    parent.last_seen = xTaskGetTickCount();
    switch (p.state) {
      case state::State::DISCONNECTED_FROM_PARENT:
      case state::State::CONNECTED_TO_PARENT: {
        state::setState(state::State::CONNECTED_TO_PARENT);
        break;
      }
      case state::State::REACHES_ROOT: {
        // this should never be the case, but we never know with malicious
        // packets
        if (!p.root.has_value()) return;

        // set root mac
        state::setRootMac(p.root.value());
        // set state
        state::setState(state::State::REACHES_ROOT);
        break;
      }
    }
    return;
  }
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::SearchProbe& p) {
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
  event::Internal::fire(event::InternalEvent::PARENT_FOUND, &data,
                        sizeof(data));
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::ConnectRequest& p) {
  if (!lastHopIsFrom(meta)) return;
  if (!reachesRoot()) return;
  if (knowsNode(meta.from)) return;
  if (!canAcceptNewChild()) return;

  // add to layout
  layout().addChild(meta.from);

  ESP_LOGI(TAG, "Child " MACSTR " connected", MAC2STR(meta.from));

  // fire connect event
  {
    meshnow_event_child_connected_t child_connected_event;
    std::copy(meta.from.addr.begin(), meta.from.addr.end(),
              child_connected_event.child_mac);
    esp_event_post(
        MESHNOW_EVENT, meshnow_event_t::MESHNOW_EVENT_CHILD_CONNECTED,
        &child_connected_event, sizeof(child_connected_event), portMAX_DELAY);
  }

  // send reply
  ESP_LOGV(TAG, "Sending Connect Response");
  send::enqueuePayload(packets::ConnectOk{state::getRootMac()},
                       send::DirectOnce(meta.from));

  // send routing table add packet upstream
  send::enqueuePayload(packets::RoutingTableAdd{meta.from},
                       send::UpstreamRetry{});
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
  event::Internal::fire(event::InternalEvent::GOT_CONNECT_RESPONSE, &data,
                        sizeof(data));
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::RoutingTableAdd& p) {
  // TODO safety checks

  ESP_LOGI(TAG, "Got Routing Table Add packet from " MACSTR " over " MACSTR,
           MAC2STR(meta.from), MAC2STR(meta.last_hop));

  if (!layout().hasChild(meta.last_hop)) return;

  // get child matching last hop
  auto& child = layout().getChild(meta.last_hop);
  child.routing_table.emplace_back(p.entry);

  // forward to parent
  // if (!layout().hasParent()) return;
  // send::enqueuePayload(packets::RoutingTableAdd{p.entry},
  //                      send::UpstreamRetry{});
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::RoutingTableRemove& p) {
  // TODO safety checks

  ESP_LOGI(TAG, "Got Routing Table Remove packet from " MACSTR " over " MACSTR,
           MAC2STR(meta.from), MAC2STR(meta.last_hop));

  if (!layout().hasChild(meta.last_hop)) return;

  // this removes the entry from the routing table of the node the packet
  // directly came from
  auto& child = layout().getChild(meta.last_hop);
  std::remove_if(child.routing_table.begin(), child.routing_table.end(),
                 [&](const auto& item) { return item.mac == p.entry; });

  // forward to parent
  if (!layout().hasParent()) return;
  send::enqueuePayload(packets::RoutingTableRemove{p.entry},
                       send::UpstreamRetry{});
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::RootUnreachable& p) {
  if (!reachesRoot()) return;
  if (!isParent(meta.last_hop)) return;

  ESP_LOGI(TAG, "Got Root Unreachable packet from parent");

  state::setState(state::State::CONNECTED_TO_PARENT);

  // forward to all children
  send::enqueuePayload(packets::RootUnreachable{}, send::DownstreamRetry{});
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::RootReachable& p) {
  if (reachesRoot()) return;
  if (!isParent(meta.last_hop)) return;

  ESP_LOGI(TAG, "Got Root Reachable packet from parent");

  state::setState(state::State::REACHES_ROOT);

  // forward to all children
  send::enqueuePayload(packets::RootUnreachable{}, send::DownstreamRetry{});
}

void PacketHandler::handle(const MetaData& meta,
                           const packets::DataFragment& p) {
  if (!isNeighbor(meta.last_hop)) return;

  // add to fragment reassembly
  fragments::addFragment(meta.from, p.frag_id, p.options.unpacked.frag_num,
                         p.options.unpacked.total_size, p.data);
}

void PacketHandler::handle(const MetaData& meta, const packets::CustomData& p) {
  // TODO safety checks

  // simply call all registered callbacks
  auto handle = custom::getFirstCBHandle();

  while (handle != nullptr) {
    // TODO const casts should not be required but i'm afraid to fix the
    // header file
    handle->cb(const_cast<uint8_t*>(meta.from.addr.data()),
               const_cast<uint8_t*>(p.data.data()), p.data.size());
    handle = handle->next;
  }
}

}  // namespace meshnow::job
