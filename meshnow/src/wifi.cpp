#include "wifi.hpp"

#include <esp_check.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>

#include "state.hpp"
#include "util/util.hpp"

static constexpr auto TAG = CREATE_TAG("Wi-Fi");

static bool should_connect_ = false;

static wifi_sta_config_t sta_config_;

static esp_event_handler_instance_t wifi_event_handler_instance_ = nullptr;
static esp_event_handler_instance_t ip_event_handler_instance_ = nullptr;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Connecting to configured AP... Nodes may not connect due to channel-scan.");
            ESP_ERROR_CHECK(esp_wifi_connect());
        } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "Connected to configured AP... Nodes may connect again.");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            auto event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
            ESP_LOGW(TAG, "Disconnected from configured AP for reason %d", event->reason);
            ESP_LOGW(TAG, "Reconnecting...");
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            auto event = static_cast<ip_event_got_ip_t *>(event_data);
            ESP_LOGI(TAG, "IP assigned from configured AP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}

namespace meshnow::wifi {

void setConfig(wifi_sta_config_t *sta_config) { sta_config_ = *sta_config; }

void setShouldConnect(bool should_connect) { should_connect_ = should_connect; }

esp_err_t init() {
    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    // set mode to station for both root and node
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Could not set Wi-Fi mode");

    // don't save config to flash
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Could not set Wi-Fi storage");
    // no powersave mode or else ESP-NOW may not receive messages
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "Could not set Wi-Fi powersave mode");

    // root may connect to a router
    if (state::isRoot() && should_connect_) {
        ESP_LOGI(TAG, "Setting up Wi-Fi for root...");

        // create default STA netif if not already created (needed for root to connect to router)
        if (esp_netif_get_handle_from_ifkey((ESP_NETIF_BASE_DEFAULT_WIFI_STA)->if_key) == nullptr) {
            // Wi-Fi station netif has not been created yet, create it
            esp_netif_create_default_wifi_sta();
            ESP_LOGI(TAG, "Created default STA interface");
        }

        // set router config
        wifi_config_t wifi_config = {.sta = sta_config_};
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

        // register event handlers
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr,
                                                                &wifi_event_handler_instance_),
                            TAG, "Could not register Wi-Fi event handler");
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr,
                                                                &ip_event_handler_instance_),
                            TAG, "Could not register IP event handler");
    }

    ESP_LOGI(TAG, "Wi-Fi initialized!");
    return ESP_OK;
}

esp_err_t deinit() {
    ESP_LOGI(TAG, "Deinitializing Wi-Fi...");

    // unregister event handlers
    if (wifi_event_handler_instance_ != nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance_), TAG,
            "Could not unregister Wi-Fi event handler");
        wifi_event_handler_instance_ = nullptr;
    }
    if (ip_event_handler_instance_ != nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler_instance_), TAG,
            "Could not unregister IP event handler");
        ip_event_handler_instance_ = nullptr;
    }

    ESP_LOGI(TAG, "Wi-Fi deinitialized!");
    return ESP_OK;
}

esp_err_t start() {
    ESP_LOGI(TAG, "Starting Wi-Fi...");

    // actually start Wi-Fi
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Could not start Wi-Fi");

    ESP_LOGI(TAG, "Wi-Fi started!");
    return ESP_OK;
}

esp_err_t stop() {
    ESP_LOGI(TAG, "Stopping Wi-Fi...");

    // unregister event handlers
    if (wifi_event_handler_instance_ != nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance_), TAG,
            "Could not unregister Wi-Fi event handler");
        wifi_event_handler_instance_ = nullptr;
    }
    if (ip_event_handler_instance_ != nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler_instance_), TAG,
            "Could not unregister IP event handler");
        ip_event_handler_instance_ = nullptr;
    }

    // stop Wi-Fi
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "Could not stop Wi-Fi");

    ESP_LOGI(TAG, "Wi-Fi stopped!");
    return ESP_OK;
}

}  // namespace meshnow::wifi
