#include "receiver.hpp"

#include <esp_log.h>

#include "packets.hpp"
#include "queue.hpp"

namespace meshnow::receive {

namespace {
constexpr auto TAG = CREATE_TAG("Receiver");
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

    // create item
    Item item{
        util::MacAddr(esp_now_info->src_addr),
        esp_now_info->rx_ctrl->rssi,
        std::move(*packet),
    };

    // push item to queue
    push(std::move(item));
}

}  // namespace meshnow::receive
