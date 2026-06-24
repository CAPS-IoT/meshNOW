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
constexpr meshnow::receive::p_level SEARCH_PROVE_LEVEL = CONFIG_SEARCH_PROBE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level SEARCH_REPLY_LEVEL = CONFIG_SEARCH_REPLY_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_REQUEST_LEVEL = CONFIG_CONNECT_REQUEST_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CONNECT_OK_LEVEL = CONFIG_CONNECT_OK_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROUTING_TABLE_ADD_LEVEL = CONFIG_ROUTING_TABLE_ADD_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROUTING_TABLE_REMOVE_LEVEL = CONFIG_ROUTING_TABLE_REMOVE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROOT_UNREACHABLE_LEVEL = CONFIG_ROOT_UNREACHABLE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level ROOT_REACHABLE_LEVEL = CONFIG_ROOT_REACHABLE_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level DATA_FRAGMENT_LEVEL = CONFIG_DATA_FRAGMENT_MESSAGE_LEVEL;
constexpr meshnow::receive::p_level CUSTOM_DATA_LEVEL = CONFIG_CUSTOM_DATA_MESSAGE_LEVEL;
}
template<typename PacketType>
void handle(Item&& item, const PacketType&) {
    constexpr p_level level =
        std::same_as<PacketType, packets::Status>             ? STATUS_LEVEL :
        std::same_as<PacketType, packets::SearchProbe>        ? SEARCH_PROVE_LEVEL :
        std::same_as<PacketType, packets::SearchReply>        ? SEARCH_REPLY_LEVEL :
        std::same_as<PacketType, packets::ConnectRequest>     ? CONNECT_REQUEST_LEVEL :
        std::same_as<PacketType, packets::ConnectOk>          ? CONNECT_OK_LEVEL :
        std::same_as<PacketType, packets::RoutingTableAdd>    ? ROUTING_TABLE_ADD_LEVEL :
        std::same_as<PacketType, packets::RoutingTableRemove> ? ROUTING_TABLE_REMOVE_LEVEL :
        std::same_as<PacketType, packets::RootUnreachable>    ? ROOT_UNREACHABLE_LEVEL :
        std::same_as<PacketType, packets::RootReachable>      ? ROOT_REACHABLE_LEVEL :
        std::same_as<PacketType, packets::DataFragment>       ? DATA_FRAGMENT_LEVEL :
                                                               0;

<<<<<<< HEAD
void Receiver::receiveCallback(const esp_now_recv_info_t *esp_now_info,
                               const uint8_t *data, int data_len) {
  // convert raw data pointer into buffer (vector) for deserialization TODO
  // avoid this
  std::vector<uint8_t> buffer(data, data + data_len);
=======
    push(std::move(item), level);
}
void Receiver::receiveCallback(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
    // convert raw data pointer into buffer (vector) for deserialization TODO avoid this
    std::vector<uint8_t> buffer(data, data + data_len);
>>>>>>> 2026/N1/messages_priorities

  // deserialize
  auto packet = packets::deserialize(buffer);

  // if deserialization failed, ignore
  // could happen because of interference with connecting to a router
  if (!packet) {
    ESP_LOGV(TAG, "Failed to deserialize packet!");
    return;
  }

<<<<<<< HEAD
  // update last seen
  update_last_seen(util::MacAddr(esp_now_info->src_addr));

  // create item
  Item item{
      util::MacAddr(esp_now_info->src_addr),
      esp_now_info->rx_ctrl->rssi,
      std::move(*packet),
  };

  // push item to queue
  push(std::move(item));
=======
    // create item
    Item item{
        util::MacAddr(esp_now_info->src_addr),
        esp_now_info->rx_ctrl->rssi,
        std::move(*packet),
    };
    if (item.packet.to == state::getThisMac() || (item.packet.to == util::MacAddr::root() && state::isRoot())) {
        handle(std::move(item), item.packet.payload);
    } else {
        //if the packet should be forwarded, this should be handled with high priority
        push(std::move(item));
    }
>>>>>>> 2026/N1/messages_priorities
}

}  // namespace meshnow::receive
