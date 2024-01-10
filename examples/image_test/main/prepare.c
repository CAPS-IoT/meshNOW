#include "prepare.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <string.h>

// Fixed MAC address of the root node
static const uint8_t root_mac[6] = {0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

// Fixed MAC address of the target node that sends the image
static const uint8_t target_mac[6] = {0x24, 0x6f, 0x28, 0x4a, 0x66, 0xcc};

static bool root = false;
static bool target = false;

static void determine_root() {
    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(my_mac, ESP_MAC_WIFI_STA));
    root = memcmp(my_mac, root_mac, 6) == 0;
}

static void determine_target() {
    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(my_mac, ESP_MAC_WIFI_STA));
    target = memcmp(my_mac, target_mac, 6) == 0;
}

static void init_flash() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void init_wifi() {
    // default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // netif
    ESP_ERROR_CHECK(esp_netif_init());

    // wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // set county
    ESP_ERROR_CHECK(esp_wifi_set_country_code("DE", true));
}

void prepare() {
    determine_root();
    determine_target();

    init_flash();
    init_wifi();

    // set light GPIO as output
    gpio_set_direction(LIGHT_GPIO, GPIO_MODE_OUTPUT);
}

bool is_root() { return root; }

bool is_target() { return target; }