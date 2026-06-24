#include "receiver.hpp"

#include <esp_log.h>
#include <utility>
#include <variant>

#include "packets.hpp"
#include "queue.hpp"
#include "layout.hpp"
#include "state.hpp"
#include "util/mac.hpp"

namespace  {
void update_last_seen(const meshnow::util::MacAddr mac) {
    auto& layout = meshnow::layout::Layout::get();

        // is child?
    if (layout.hasChild(mac)) {
        auto& child = layout.getChild(mac);
        child.last_seen = xTaskGetTickCount();
    }

    // is parent?
    if (layout.hasParent()) {
        auto& parent = layout.getParent();
        if (parent.mac != mac) return;

        parent.last_seen = xTaskGetTickCount();
    }
}
}

namespace meshnow::receive {

namespace {
constexpr auto TAG = CREATE_TAG("Receiver");
constexpr meshnow::receive::p_level STATUS_LEVEL = CONFIG_STATUS_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level SEARCH_PROBE_LEVEL = CONFIG_SEARCH_PROBE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level SEARCH_REPLY_LEVEL = CONFIG_SEARCH_REPLY_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_REQUEST_LEVEL = CONFIG_CONNECT_REQUEST_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_OK_LEVEL = CONFIG_CONNECT_OK_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_OK_ACK_LEVEL = CONFIG_CONNECT_OK_ACK_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_END_LEVEL = CONFIG_CONNECT_END_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROUTING_TABLE_ADD_LEVEL = CONFIG_ROUTING_TABLE_ADD_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROUTING_TABLE_REMOVE_LEVEL = CONFIG_ROUTING_TABLE_REMOVE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROOT_UNREACHABLE_LEVEL = CONFIG_ROOT_UNREACHABLE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROOT_REACHABLE_LEVEL = CONFIG_ROOT_REACHABLE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level DATA_FRAGMENT_LEVEL = CONFIG_DATA_FRAGMENT_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CUSTOM_DATA_LEVEL = CONFIG_CUSTOM_DATA_MESSAGE_LEVEL;
}
template<typename PacketType>
void handle(Item&& item, const PacketType&) {
    p_level level = [] {
        if constexpr (std::same_as<PacketType, packets::Status>) {
            ESP_LOGD(TAG, "Receiving STATUS : pushing at level %d", STATUS_LEVEL);
            return STATUS_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::SearchProbe>) {
            ESP_LOGD(TAG, "Receiving SEARCH_PROBE : pushing at level %d", SEARCH_PROBE_LEVEL);
            return SEARCH_PROBE_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::SearchReply>) {
            ESP_LOGD(TAG, "Receiving SEARCH_REPLY : pushing at level %d", SEARCH_REPLY_LEVEL);
            return SEARCH_REPLY_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::ConnectRequest>) {
            ESP_LOGD(TAG, "Receiving CONNECT_REQUEST : pushing at level %d", CONNECT_REQUEST_LEVEL);
            return CONNECT_REQUEST_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::ConnectOk>) {
            ESP_LOGD(TAG, "Receiving CONNECT_OK : pushing at level %d", CONNECT_OK_LEVEL);
            return CONNECT_OK_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::ConnectOkAck) {
            ESP_LOGD(TAG, "Receiving CONNECT_OK_ACK : pushing at level %d", CONNECT_OK_ACK_LEVEL);
            return CONNECT_OK_ACK_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::ConnectEnd) {
            ESP_LOGD(TAG, "Receiving CONNECT_ENT : pushing at level %d", CONNECT_END_LEVEL);
            return CONNECT_END_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::RoutingTableAdd>) {
            ESP_LOGD(TAG, "Receiving ROUTING_ADD_TABLE : pushing at level %d", ROUTING_TABLE_ADD_LEVEL);
            return ROUTING_TABLE_ADD_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::RoutingTableRemove>) {
            ESP_LOGD(TAG, "Receiving ROUTING_REM_TABLE : pushing at level %d", ROUTING_TABLE_REMOVE_LEVEL);
            return ROUTING_TABLE_REMOVE_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::RootUnreachable>) {
            ESP_LOGD(TAG, "Receiving ROOT_UNREACHABLE : pushing at level %d", ROOT_UNREACHABLE_LEVEL);
            return ROOT_UNREACHABLE_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::RootReachable>) {
            ESP_LOGD(TAG, "Receiving ROOT_REACHABLE : pushing at level %d", ROOT_REACHABLE_LEVEL);
            return ROOT_REACHABLE_LEVEL;
        }
        else if constexpr (std::same_as<PacketType, packets::DataFragment>) {
            ESP_LOGD(TAG, "Receiving DATAFRAGM : pushing at level %d", DATA_FRAGMENT_LEVEL);
            return DATA_FRAGMENT_LEVEL;
        }
        else {
            ESP_LOGD(TAG, "Receiving UNKNOW : pushing at level 0\n");
            return 0;
        }
    }();
    ESP_LOGD(TAG, "Pushing message to level %d", level);
    push(std::move(item), level);
}
void Receiver::receiveCallback(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    // convert raw data pointer into buffer (vector) for deserialization TODO avoid this
    std::vector<uint8_t> buffer(data, data + data_len);

    // deserialize
    auto packet = packets::deserialize(buffer);

    // if deserialization failed, ignore
    // could happen because of interference with connecting to a router
    if (!packet) {
        ESP_LOGV(TAG, "Failed to deserialize packet!");
        return;
    }

    // update last seen
    update_last_seen(util::MacAddr(esp_now_info->src_addr));
    // create item
    Item item{
        util::MacAddr(esp_now_info->src_addr),
        esp_now_info->rx_ctrl->rssi,
        std::move(*packet),
    };
    if (item.packet.to == state::getThisMac() || (item.packet.to == util::MacAddr::root() && state::isRoot())) {
        std::visit([&](const auto& payload) { handle(std::move(item), payload); }, item.packet.payload);    
    } else {
        //if the packet should be forwarded, this should be handled with high priority
        push(std::move(item));
    }
}

}  // namespace meshnow::receive
