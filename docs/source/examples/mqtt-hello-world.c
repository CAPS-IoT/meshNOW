#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <meshnow.h>
#include <mqtt_client.h>
#include <nvs_flash.h>

// The Wi-Fi credentials are configured via sdkconfig
#define ROUTER_SSID CONFIG_EXAMPLE_ROUTER_SSID
#define ROUTER_PASS CONFIG_EXAMPLE_ROUTER_PASSWORD

// MQTT broker URI is configured via sdkconfig
#define MQTT_BROKER_URI CONFIG_EXAMPLE_MQTT_BROKER_URI

// Waitbits
#define GOT_IP_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

static const char *TAG = "hello-world";

// Fixed MAC address of the root node
static const uint8_t root_mac[MESHNOW_ADDRESS_LENGTH] = {0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

// MAC address of the current node
static uint8_t node_mac[MESHNOW_ADDRESS_LENGTH];

// Whether this node is the root
static bool is_root = false;

// Event group for various waiting processes
static EventGroupHandle_t my_event_group;

// Event handler for IP_EVENT
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == IP_EVENT);
    assert(event_id == IP_EVENT_STA_GOT_IP);

    // set the bit for getting IP
    xEventGroupSetBits(my_event_group, GOT_IP_BIT);
}

// Event handler for MQTT_EVENT
static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_id == MQTT_EVENT_CONNECTED);

    xEventGroupSetBits(my_event_group, MQTT_CONNECTED_BIT);
}

// Sends messages via MQTT
static void perform_mqtt() {
    // initialize
    esp_mqtt_client_config_t config = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init();

    // register event handler
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_CONNECTED, &mqtt_event_handler, NULL));

    char *msg;
    asprintf(&msg, "Hello World from " MACSTR "(%s)", MAC2STR(node_mac), is_root ? "root" : "node");
    size_t len = strlen(msg);

    // start
    ESP_ERROR_CHECK(esp_mqtt_client_start(client, &config));

    // wait for connection
    EventBits_t bits = xEventGroupWaitBits(my_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & MQTT_CONNECTED_BIT) != 0);

    while (true) {
        ESP_ERROR_CHECK(esp_mqtt_client_publish(client, "/helloworld", msg, len, 0, 0));
        vTaskDelay(pMS_TO_TICKS(5000));
    }
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

    // check if root
    is_root = memcmp(node_mac, root_mac, MESHNOW_ADDRESS_LENGTH) == 0;

    // create event group
    my_event_group = xEventGroupCreate();
    assert(my_event_group != NULL);

    // initialize meshnow
    wifi_sta_config_t sta_config = {
        .ssid = ROUTER_SSID,
        .password = ROUTER_PASS,
    };
    meshnow_config_t config = {
        .root = is_root,
        .router_config =
            {
                .should_connect = true,  // MQTT requires internet access
                .sta_config = sta_config,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // register event handler for IP_EVENT
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());

    // wait for IP
    // when we get an IP address, the node has to have connected to a parent/router
    // in a real application, you would want to handle disconnects/lost IP as well and restart MQTT
    EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & GOT_IP_BIT) != 0);

    perform_mqtt();
}
