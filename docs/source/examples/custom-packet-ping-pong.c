#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <meshnow.h>
#include <nvs_flash.h>

// Waitbits
#define CONNECTED_BIT BIT0

static const char *TAG = "ping-pong";

// Fixed MAC address of the root node
static const uint8_t root_mac[MESHNOW_ADDRESS_LENGTH] = {0x24, 0x6f, 0x28, 0x4a, 0x63, 0x3c};

// MAC address of the current node
static uint8_t node_mac[MESHNOW_ADDRESS_LENGTH];

// Whether this node is the root
static bool is_root = false;

// Event group for various waiting processes
static EventGroupHandle_t my_event_group;

// Custom packet types
typedef enum {
    PING = 0,
    PONG = 1,
} custom_packet_type_t;

// Custom packet for ping-ponging
typedef struct {
    custom_packet_type_t type;
    uint32_t timestamp;
} __attribute__((__packed__)) custom_packet_t;

// Event handler for MESHNOW_EVEMT
static void meshnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_base == MESHNOW_EVENT);

    switch (event_id) {
        case MESHNOW_EVENT_PARENT_CONNECTED: {
            meshnow_event_parent_connected_t *data = event_data;
            ESP_LOGI(TAG, "Parent connected: " MACSTR, MAC2STR(data->parent_mac));
            xEventGroupSetBits(my_event_group, CONNECTED_BIT);
            break;
        }
        case MESHNOW_EVENT_PARENT_DISCONNECTED: {
            meshnow_event_parent_disconnected_t *data = event_data;
            ESP_LOGI(TAG, "Parent disconnected: " MACSTR, MAC2STR(data->parent_mac));
            xEventGroupClearBits(my_event_group, CONNECTED_BIT);
            break;
        }
    }
}

// Sends custom ping messages to every node in the mesh
static void perform_ping() {
    // since we are root, we can send right away
    while (true) {
        custom_packet_t data = {
            .type = PING,
            .timestamp = xTaskGetTickCount(),
        };
        ESP_ERROR_CHECK(meshnow_send(&MESHNOW_BROADCAST_ADDRESS, (uint8_t *)&data, sizeof(data));
        vTaskDelay(pMS_TO_TICKS(5000));
    }
}

// Callback, reacts to pings and pongs depending on the node type
static void my_data_callback(uint8_t *src, uint8_t *buffer, size_t len) {
    // usually, you would do input sanitization here
    // we skip it for the sake of simplicity

    // cast buffer to custom packet
    custom_packet_t *data = (custom_packet_t *)buffer;

    ESP_LOGI(TAG, "Received packet of type %d from " MACSTR " with timestamp %lu", data->type, MAC2STR(src),
             data->timestamp);

    // if we are not the root, we send a pong
    if (!is_root) {
        custom_packet_t pong = {
            .type = PONG,
            .timestamp = data->timestamp,
        };
        ESP_ERROR_CHECK(meshnow_send(src, (uint8_t *)&pong, sizeof(pong)));
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
    meshnow_config_t config = {
        .root = is_root,
        .router_config =
            {
                .should_connect = fale,  // do not connect to internet, we want to use inter-node communication
                .sta_config = NULL,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // register event handler for MESHNOW_EVENT
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(MESHNOW_EVENT, ESP_EVENT_ANY_IID, &meshnow_event_handler, NULL, NULL));

    // register data callback
    meshnow_data_cb_handle_t data_cb_handle;  // unused, as we do not unregister the callback
    ESP_ERROR_CHECK(meshnow_register_data_cb(&my_data_callback, &data_cb_handle));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());

    // wait for IP
    // when we get an IP address, the node has to have connected to a parent/router
    // in a real application, you would want to handle disconnects/lost IP as well and restart MQTT
    EventBits_t bits = xEventGroupWaitBits(my_event_group, GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & GOT_IP_BIT) != 0);

    if (is_root) {
        perform_ping();
    }
}
