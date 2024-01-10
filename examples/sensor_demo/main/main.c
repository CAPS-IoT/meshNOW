#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <meshnow.h>
#include <nvs_flash.h>
#include <string.h>
#include "garden.h"

// The Wi-Fi credentials are configured via sdkconfig
#define ROUTER_SSID CONFIG_EXAMPLE_ROUTER_SSID
#define ROUTER_PASS CONFIG_EXAMPLE_ROUTER_PASSWORD

// Waitbits
#define GOT_IP_BIT BIT0

static const char *TAG = "main";

// Fixed MAC address of the root node
static const uint8_t root_mac[MESHNOW_ADDRESS_LENGTH] = {0x24, 0x6f, 0x28, 0x4a, 0x66, 0xcc};
// Fixed MAC address of the garden node
static const uint8_t garden_mac[MESHNOW_ADDRESS_LENGTH] = {0xbc, 0xdd, 0xc2, 0xcc, 0xc3, 0x0c};

static bool is_root_node = false;
static bool is_garden_node = false;

// MAC address of the current node
static uint8_t node_mac[MESHNOW_ADDRESS_LENGTH];

// Event group for various waiting processes
static EventGroupHandle_t my_event_group;

// IP handlers
static void got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    // set the bit for getting IP
    xEventGroupSetBits(my_event_group, GOT_IP_BIT);
}
static void lost_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "Lost IP");
    abort();
}

// Initializes NVS, Event Loop, Wi-Fi, and Netif
static void pre_init(void) {
    // nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // netif
    ESP_ERROR_CHECK(esp_netif_init());
}

void app_main(void) {
    pre_init();

    // get MAC address
    ESP_ERROR_CHECK(esp_read_mac(node_mac, ESP_MAC_WIFI_STA));

    is_root_node =  memcmp(node_mac, root_mac, MESHNOW_ADDRESS_LENGTH) == 0;
    is_garden_node = memcmp(node_mac, garden_mac, MESHNOW_ADDRESS_LENGTH) == 0;

    // create event group
    my_event_group = xEventGroupCreate();
    assert(my_event_group != NULL);

    // initialize meshnow
    wifi_sta_config_t sta_config = {
        .ssid = ROUTER_SSID,
        .password = ROUTER_PASS,
    };
    meshnow_config_t config = {
        .root = is_root_node,
        .router_config =
            {
                .should_connect = true,  // MQTT requires internet access
                .sta_config = &sta_config,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // ip handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &lost_ip_handler, NULL, NULL));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());

    // wait for IP
    // when we get an IP address, the node has to have connected to a parent/router
    EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & GOT_IP_BIT) != 0);

    if(is_garden_node) {
        perform_garden();
    }
}
