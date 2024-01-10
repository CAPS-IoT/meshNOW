#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <meshnow.h>
#include <sdkconfig.h>

#include "demo_mqtt.h"
#include "prepare.h"

// Waitbits
#define GOT_IP_BIT BIT0
#define ASSIGNED_IP_BIT BIT1

static const char *TAG = "image_test";

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

static void meshnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_Data) {
    assert(event_base == MESHNOW_EVENT);

    switch (event_id) {
        case MESHNOW_EVENT_PARENT_CONNECTED:
            ESP_LOGI(TAG, "Parent connected");
            gpio_set_level(LIGHT_GPIO, 1);
            break;
        case MESHNOW_EVENT_PARENT_DISCONNECTED:
            ESP_LOGI(TAG, "Parent disconnected");
            gpio_set_level(LIGHT_GPIO, 0);
            break;
        default:
            ESP_LOGW(TAG, "Unknown event: %ld", event_id);
            break;
    }
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

    // register MeshNOW handler to blink light when connected
    ESP_ERROR_CHECK(esp_event_handler_instance_register(MESHNOW_EVENT, MESHNOW_EVENT_PARENT_CONNECTED,
                                                        &meshnow_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(MESHNOW_EVENT, MESHNOW_EVENT_PARENT_DISCONNECTED,
                                                        &meshnow_event_handler, NULL, NULL));

    // create config for connecting to the router
    wifi_sta_config_t sta_config = {
        .ssid = CONFIG_AP_SSID,
        .password = CONFIG_AP_PASSWD,
    };

    // initialize meshnow
    meshnow_config_t config = {
        .root = is_root(),
        .router_config =
            {
                .should_connect = true,
                .sta_config = &sta_config,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());

    // enable LR
    uint8_t protocol = WIFI_PROTOCOL_LR;
    if (is_root()) {
        protocol |= WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
    }
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, protocol));
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (mode == WIFI_MODE_APSTA || mode == WIFI_MODE_AP) {
        ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_AP, protocol));
    }

    if (!is_root()) {
        ESP_LOGI(TAG, "Waiting for my IP");
        // wait to get IP from root
        EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        assert((bits & GOT_IP_BIT) != 0);

        ESP_LOGI(TAG, "Got my IP, continuing");

        // the node should now try to send the image
        if (is_target()) {
            start_mqtt();
        }
    } else {
        ESP_LOGI(TAG, "Waiting for node to connect");
        // wait for the node to connect
        EventBits_t bits = xEventGroupWaitBits(my_event_group, ASSIGNED_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        assert((bits & ASSIGNED_IP_BIT) != 0);

        ESP_LOGI(TAG, "Node connected, continuing");
    }
}
