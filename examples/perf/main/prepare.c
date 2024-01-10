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

#define IMPL_GPIO GPIO_NUM_18
#define LR_GPIO GPIO_NUM_19

// Fixed MAC address of the root node
static const uint8_t root_mac[6] = {0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

static bool root = false;

static void determine_root() {
    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(my_mac, ESP_MAC_WIFI_STA));
    root = memcmp(my_mac, root_mac, 6) == 0;
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
}

void prepare() {
    determine_root();

    init_flash();
    init_wifi();
}

setup_config_t get_setup_config() {
    static bool initialized = false;
    static setup_config_t config;

    if (!initialized) {
        gpio_set_direction(IMPL_GPIO, GPIO_MODE_INPUT);
        gpio_set_direction(LR_GPIO, GPIO_MODE_INPUT);
        gpio_set_pull_mode(IMPL_GPIO, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(LR_GPIO, GPIO_PULLUP_ONLY);

        config.impl = gpio_get_level(IMPL_GPIO) ? MESHNOW : ESPMESH;
        config.long_range = !gpio_get_level(LR_GPIO);

        initialized = true;
    }

    return config;
}

bool is_root() { return root; }