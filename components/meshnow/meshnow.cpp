#include "meshnow.hpp"

#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "error.hpp"
#include "internal.hpp"
#include "networking.hpp"

static const char *TAG = CREATE_TAG("🦌");

static void checkWiFi() {
    // check if WiFi is initialized
    // do dummy operation
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WiFi is not initialized");
        CHECK_THROW(ret);
    } else {
        ESP_LOGI(TAG, "WiFi OK!");
    }
}

static void checkEspNow() {
    // check if ESP-NOW is initialized
    // do dummy operation
    esp_now_peer_info_t peer;
    esp_err_t ret = esp_now_fetch_peer(true, &peer);
    if (ret == ESP_ERR_ESPNOW_NOT_INIT) {
        ESP_LOGE(TAG, "ESP-NOW is not initialized");
        CHECK_THROW(ret);
    } else {
        ESP_LOGI(TAG, "ESP-NOW OK!");
        ESP_LOGI(TAG, "You may register the MeshNow callbacks now...");
    }
}

static void checkNetif() {
    ESP_LOGW(TAG,
             "Cannot check if Netif is initialized due to technical limitations.\n"
             "Please make sure to have called esp_netif_init() exactly once before initializing MeshNOW.\n"
             "Otherwise, the device might crash due to Netif/LWIP errors.");
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to WiFi...");
    }
    else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "Retrying to connect...");

    } else if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
        auto *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "ESP32's IP=" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void setupWiFi(meshnow::Config config) {
    ESP_LOGI(TAG, "Setting WiFi configuration");

    CHECK_THROW(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    CHECK_THROW(esp_wifi_set_mode(WIFI_MODE_STA));
    if (config.root) {
        esp_netif_create_default_wifi_sta();
        wifi_config_t wifi_config = {.sta = config.sta_config};
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

        CHECK_THROW(esp_event_handler_instance_register(WIFI_EVENT,
                                                  ESP_EVENT_ANY_ID,
                                                  &wifi_event_handler,
                                                  nullptr,
                                                  nullptr));
        CHECK_THROW(esp_event_handler_instance_register(IP_EVENT,
                                                  IP_EVENT_STA_GOT_IP,
                                                  &wifi_event_handler,
                                                  nullptr,
                                                  nullptr));
    }
    CHECK_THROW(esp_wifi_start());

    // no powersaving
    // TODO maybe turn this on actually?
    CHECK_THROW(esp_wifi_set_ps(WIFI_PS_NONE));
    // use long range mode -> up to 1km according to espressif
    // TODO this also needs to be different for root (LR + BGN)
//    CHECK_THROW(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));

    ESP_LOGI(TAG, "WiFi set up");
}

using meshnow::Mesh;

Mesh::Mesh(const Config config)
    : config_{config}, state_{std::make_shared<NodeState>(config_.root)}, networking_{state_} {
    ESP_LOGI(TAG, "Initializing MeshNOW");
    ESP_LOGI(TAG, "Checking ESP-IDF network stack is properly initialized...");
    checkWiFi();
    checkEspNow();
    checkNetif();
    ESP_LOGI(TAG, "Check OK!");
    setupWiFi(config_);
    ESP_LOGI(TAG, "MeshNOW initialized. You can start the mesh now 🦌");
}

Mesh::~Mesh() {
    ESP_LOGI(TAG, "Deinitializing MeshNOW");

    if (state_->isStarted()) {
        ESP_LOGW(TAG, "The mesh is still running. Stopping it for you. Consider calling stop() yourself! >:(");
        try {
            stop();
        } catch (const std::exception &e) {
            ESP_LOGE(TAG, "Error while stopping MeshNOW: %s", e.what());
            ESP_LOGE(TAG, "This should never happen. Terminating....");
            std::terminate();
        }
    }

    ESP_LOGI(TAG, "MeshNOW deinitialized. Goodbye 👋");
}

void Mesh::start() {
    auto lock = state_->acquireLock();

    if (state_->isStarted()) {
        ESP_LOGE(TAG, "MeshNOW is already running");
        throw AlreadyStartedException();
    }

    ESP_LOGI(TAG, "Starting MeshNOW as %s...", state_->isRoot() ? "root" : "node");

    networking_.start();

    ESP_LOGI(TAG, "Liftoff! 🚀");

    state_->setStarted(true);
}

void Mesh::stop() {
    auto lock = state_->acquireLock();

    if (!state_->isStarted()) {
        ESP_LOGE(TAG, "MeshNOW is not running");
        throw NotStartedException();
    }

    ESP_LOGI(TAG, "Stopping MeshNOW as %s...", state_->isRoot() ? "root" : "node");

    networking_.stop();

    ESP_LOGI(TAG, "Mesh stopped! 🛑");

    state_->setStarted(false);
}

static void recvCbWrapper(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len, void *arg) {
    static_cast<const meshnow::Networking *>(arg)->main_worker_->onReceive(esp_now_info, data, data_len);
}

static void sendCbWrapper(const uint8_t *mac_addr, esp_now_send_status_t status, void *arg) {
    static_cast<const meshnow::Networking *>(arg)->send_worker_->onSend(mac_addr, status);
}

meshnow::Callbacks Mesh::getCallbacks() const {
    // cast Networking class to void* and pass it along as to retain its data
    return Callbacks{static_cast<void *>(const_cast<Networking *>(&networking_)), recvCbWrapper, sendCbWrapper};
}