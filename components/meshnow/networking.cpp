#include "networking.hpp"

#include <esp_log.h>
#include <esp_now.h>

#include <cstdint>
#include <vector>

#include "constants.hpp"
#include "error.hpp"
#include "internal.hpp"
#include "packets.hpp"

static const char* TAG = CREATE_TAG("Networking");

// TODO handle list of peers full
static void add_peer(const meshnow::MAC_ADDR& mac_addr) {
    ESP_LOGI(TAG, "Adding peer " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
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

void meshnow::Networking::rawSend(const MAC_ADDR& mac_addr, const std::vector<uint8_t>& data) {
    if (data.size() > MAX_RAW_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload size %d exceeds maximum data size %d", data.size(), MAX_RAW_PACKET_SIZE);
        throw PayloadTooLargeException();
    }

    // TODO delete unused peers first
    add_peer(mac_addr);
    ESP_LOGI(TAG, "Sending raw data to " MAC_FORMAT, MAC_FORMAT_ARGS(mac_addr));
    CHECK_THROW(esp_now_send(mac_addr.data(), data.data(), data.size()));
}

void meshnow::Networking::onSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // TODO
    ESP_LOGI(TAG, "Send status: %d", status);
    // notify send worker
    send_worker_.sendFinished(status == ESP_NOW_SEND_SUCCESS);
}

void meshnow::Networking::onReceive(const esp_now_recv_info_t* esp_now_info, const uint8_t* data, int data_len) {
    ESP_LOGI(TAG, "Received data");
    // TODO error checking and timeout

    ReceiveMeta meta{};
    // copy everything because the pointers are only valid during this function call
    std::copy(esp_now_info->src_addr, esp_now_info->src_addr + sizeof(MAC_ADDR), meta.src_addr.begin());
    std::copy(esp_now_info->des_addr, esp_now_info->des_addr + sizeof(MAC_ADDR), meta.dest_addr.begin());
    meta.rssi = esp_now_info->rx_ctrl->rssi;

    auto buffer = std::vector<uint8_t>(data, data + data_len);
    auto payload = packets::Packet::Packet::deserialize(buffer);
    if (!payload) {
        // received invalid payload
        return;
    }

    // this will in turn call one of the handle functions
    payload->handle(*this, meta);
}

void meshnow::Networking::handleStillAlive(const ReceiveMeta& meta) {}

void meshnow::Networking::handleAnyoneThere(const ReceiveMeta& meta) {
    // TODO check if already connected or cannot accept any more children
    ESP_LOGI(TAG, "Sending I am here");
    send_worker_.enqueuePayload(meta.src_addr, std::make_unique<packets::IAmHerePayload>());
}

void meshnow::Networking::handleIAmHere(const ReceiveMeta& meta) {
    // TODO collect multiple responses and select the best one based on RSSI
    // TODO synchronization with current state
    ESP_LOGI(TAG, "Sending pls connect");
    send_worker_.enqueuePayload(meta.src_addr, std::make_unique<packets::PlsConnectPayload>());
}

void meshnow::Networking::handlePlsConnect(const ReceiveMeta& meta) {
    // TODO need some reservation/synchronization mechanism so we don not allocate the same "child slot" to multiple
    // nodes
    // TODO add child information
    ESP_LOGI(TAG, "Sending welcome");
    send_worker_.enqueuePayload(meta.src_addr, std::make_unique<packets::WelcomePayload>());
    // TODO send node connected event to parent
}

void meshnow::Networking::handleWelcome(const ReceiveMeta& meta) {
    // TODO synchronization
    // TODO add parent information
    ESP_LOGI(TAG, "Got welcome!");
}

void meshnow::Networking::handleNodeConnected(const ReceiveMeta& meta, const packets::NodeConnectedPayload& payload) {}

void meshnow::Networking::handleNodeDisconnected(const ReceiveMeta& meta,
                                                 const packets::NodeDisconnectedPayload& payload) {}

void meshnow::Networking::handleMeshUnreachable(const ReceiveMeta& meta) {}

void meshnow::Networking::handleMeshReachable(const ReceiveMeta& meta) {}

void meshnow::Networking::handleDataAck(const ReceiveMeta& meta, const packets::DataAckPayload& payload) {}

void meshnow::Networking::handleDataNack(const ReceiveMeta& meta, const packets::DataNackPayload& payload) {}

void meshnow::Networking::handleDataFirst(const ReceiveMeta& meta, const packets::DataFirstPayload& payload) {}

void meshnow::Networking::handleDataNext(const ReceiveMeta& meta, const packets::DataNextPayload& payload) {}
