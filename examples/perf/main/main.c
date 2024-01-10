#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <iperf.h>

#include "espmesh_runner.h"
#include "meshnow_runner.h"
#include "prepare.h"

// Waitbits
#define GOT_IP_BIT BIT0
#define ASSIGNED_IP_BIT BIT1

static const char *TAG = "perf";

// Event group for various waiting processes
static EventGroupHandle_t my_event_group;

// IP addresses: mine, connected node
static esp_ip4_addr_t my_ip, node_ip;

// Event handler for IP_EVENT
static void got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);
    assert(event_id == IP_EVENT_STA_GOT_IP);

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    my_ip = event->ip_info.ip;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&my_ip));

    // set the bit for getting IP
    xEventGroupSetBits(my_event_group, GOT_IP_BIT);
}

// Event handler for IP_EVENT_AP_STAIPASSIGNED
static void assigned_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);
    assert(event_id == IP_EVENT_AP_STAIPASSIGNED);

    ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
    node_ip = event->ip;

    ESP_LOGI(TAG, "Node connected with IP: " IPSTR, IP2STR(&node_ip));

    // set the bit for assigning IP
    xEventGroupSetBits(my_event_group, ASSIGNED_IP_BIT);
}

static void perform_iperf() {
    ESP_LOGI(TAG, "Starting iperf %s", is_root() ? "client" : "server");

    iperf_cfg_t cfg;
    cfg.flag = is_root() ? IPERF_FLAG_CLIENT : IPERF_FLAG_SERVER;
    // TCP since we want a measurement for MQTT, HTTP, etc.
    cfg.flag |= IPERF_FLAG_TCP;
    // only IPv4 is supported
    cfg.type = IPERF_IP_TYPE_IPV4;
    // set IP addresses
    if (is_root()) {
        cfg.destination_ip4 = node_ip.addr;
    }
    cfg.source_ip4 = my_ip.addr;
    // default port
    cfg.dport = cfg.sport = IPERF_DEFAULT_PORT;

    // default measure settings
    cfg.interval = 3;
    cfg.time = 30;
    cfg.len_send_buf = 0;
    cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;

    iperf_start(&cfg);
}

void app_main(void) {
    prepare();

    // create event group
    my_event_group = xEventGroupCreate();
    assert(my_event_group != NULL);

    // register IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &assigned_ip_handler, NULL, NULL));

    setup_config_t config = get_setup_config();
    ESP_LOGE(TAG, "RUNNING %s WITH LR=%s", config.impl == MESHNOW ? "meshnow" : "esp-wifi-mesh",
             config.long_range ? "true" : "false");

    //    if (config.long_range) {
    //        ESP_ERROR_CHECK(esp_wifi_set_protocol(
    //            ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
    //        ESP_ERROR_CHECK(esp_wifi_set_protocol(
    //            ESP_IF_WIFI_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
    //    }

    // run meshnow/esp-wifi-mesh
    if (config.impl == MESHNOW) {
        run_meshnow();
    } else {
        run_espmesh();
    }

    if (config.long_range) {
        ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR));
        wifi_mode_t mode;
        ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
        if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP) {
            ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_AP, WIFI_PROTOCOL_LR));
        }
    }

    if (!is_root()) {
        ESP_LOGI(TAG, "Waiting for my IP");
        // wait to get IP from root
        EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        assert((bits & GOT_IP_BIT) != 0);

        ESP_LOGI(TAG, "Got my IP, continuing");

        // we can now start the iperf server as the node
        perform_iperf();
    } else {
        ESP_LOGI(TAG, "Waiting for node to connect");
        // wait for the node to connect
        EventBits_t bits = xEventGroupWaitBits(my_event_group, ASSIGNED_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        assert((bits & ASSIGNED_IP_BIT) != 0);

        ESP_LOGI(TAG, "Node connected, continuing");

        // wait to make sure the node has started its iperf server
        vTaskDelay(pdMS_TO_TICKS(1000));

        // can now start the iperf client
        perform_iperf();
    }
}
