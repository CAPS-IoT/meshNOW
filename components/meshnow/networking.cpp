#include "networking.hpp"

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_now.h>

#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "handshaker.hpp"
#include "internal.hpp"
#include "packets.hpp"
#include "receive_meta.hpp"
#include "state.hpp"

static const char* TAG = CREATE_TAG("Networking");

meshnow::MAC_ADDR meshnow::Networking::queryThisMac() {
    meshnow::MAC_ADDR mac;
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGV(TAG, "Adding peer " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    if (esp_now_is_peer_exist(mac_addr.data())) {
        return;
    }
    esp_now_peer_info_t peer_info{};
    peer_info.channel = 0;
    peer_info.encrypt = false;
    peer_info.ifidx = WIFI_IF_STA;
    std::copy(mac_addr.begin(), mac_addr.end(), peer_info.peer_addr);
    CHECK_THROW(esp_now_add_peer(&peer_info));
}

meshnow::Networking::Networking(meshnow::NodeState& state)
    : state_{state}, send_worker_{*this}, routing_info_{queryThisMac()} {
    packet_handlers_.push_back(std::make_unique<Handshaker>(send_worker_, state_, routing_info_));
}

void meshnow::Networking::start() {
    if (!state_.isRoot()) {
        ESP_LOGI(TAG, "Ready to connect!");
    } else {
        // update the routing info. Add our own MAC as the root MAC
        routing_info_.setRoot(queryThisMac());
        // the root can always reach itself
        state_.setConnectionStatus(NodeState::ConnectionStatus::ROOT_REACHABLE);
    }
}

void meshnow::Networking::rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data) {
    if (data.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum data size %d", data.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    // TODO delete unused peers first
    add_peer(mac_addr);
    ESP_LOGV(TAG, "Sending raw data to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), data.data(), data.size()));
}

void meshnow::Networking::onSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGD(TAG, "Send status: %d", status);
    // notify send worker
    send_worker_.sendFinished(status == ESP_NOW_SEND_SUCCESS);
}

void meshnow::Networking::onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGV(TAG, "Received data");
    // TODO error checking and timeout

    ReceiveMeta meta{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), meta.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), meta.dest_addr.begin());
    meta.rssi = esp_now_info->rx_ctrl->rssi;

    std::vector<uint8_t> buffer(data, data + data_len);
    auto packet = meshnow::packets::deserialize(buffer);
    if (!packet) {
        // received invalid payload
        return;
    }

    // add sequence number to meta
    meta.seq_num = packet->seq_num;

    // call the corresponding handlers
    for (auto& handler : packet_handlers_) {
        if (handler->handlePacket(meta, packet->payload)) {
            break;
        }
    }
}

void meshnow::Networking::handle(const ReceiveMeta& meta, const packets::NodeConnected& p) {
    // add to routing table
    routing_info_.addToRoutingTable(meta.src_addr, p.child_mac);
    // forward to parent, if not root
    if (!state_.isRoot()) {
        send_worker_.enqueuePacket(routing_info_.getParentMac(), packets::Packet{0, p});
    }
}
