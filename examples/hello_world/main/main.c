#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
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
static const uint8_t root_mac[MESHNOW_ADDRESS_LENGTH] = {0xb8, 0xd6, 0x1a, 0x5a, 0x27, 0xec};

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

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    // set the bit for getting IP
    xEventGroupSetBits(my_event_group, GOT_IP_BIT);
}

// Event handler for MQTT_EVENT
static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    assert(event_id == MQTT_EVENT_CONNECTED);

    xEventGroupSetBits(my_event_group, MQTT_CONNECTED_BIT);
}

static void send_info(esp_mqtt_client_handle_t client) {
    char *msg;

    size_t visible_mesh_size = 0;
    meshnow_visible_mesh_size(&visible_mesh_size);
    meshnow_addr_t parent_mac = {};
    bool has_parent = false;
    meshnow_get_parent(parent_mac, &has_parent);

    asprintf(&msg,
             "Info of: " MACSTR
             " (%s)\n"
             "Visible mesh size: %d\n"
             "Has parent: %d\n | Parent MAC: " MACSTR
             "\n"
             "Connected Nodes:",
             MAC2STR(node_mac), is_root ? "root" : "node", visible_mesh_size, has_parent, MAC2STR(parent_mac));

    size_t child_num = 0;
    meshnow_get_children_num(&child_num);
    meshnow_addr_t *child_macs = malloc(child_num * sizeof(meshnow_addr_t));
    meshnow_get_children(child_macs, &child_num);

    for (size_t i = 0; i < child_num; i++) {
        // get children of child
        size_t child_children_num = 0;
        meshnow_get_child_children_num(child_macs[i], &child_children_num);
        meshnow_addr_t *child_children_macs = malloc(child_children_num * sizeof(meshnow_addr_t));
        meshnow_get_child_children(child_macs[i], child_children_macs, &child_children_num);

        // add child along with its children to the message
        char *child_msg;
        asprintf(&child_msg, " " MACSTR, MAC2STR(child_macs[i]));
        for (size_t j = 0; j < child_children_num; j++) {
            // add one of the child's children
            char *child_child_msg;
            asprintf(&child_child_msg, " " MACSTR, MAC2STR(child_children_macs[j]));
            child_msg = realloc(child_msg, strlen(child_msg) + strlen(child_child_msg) + 1);
            strcat(child_msg, child_child_msg);
            free(child_child_msg);
        }

        msg = realloc(msg, strlen(msg) + strlen(child_msg) + 1);
        strcat(msg, child_msg);
        free(child_msg);

        free(child_children_macs);
    }

    ESP_ERROR_CHECK(esp_mqtt_client_publish(client, "/info", msg, strlen(msg), 0, 0));

    free(child_macs);
    free(msg);
}

// Sends messages via MQTT
static void perform_mqtt() {
    // initialize
    esp_mqtt_client_config_t config = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config);

    // register event handler
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_CONNECTED, &mqtt_event_handler, NULL));

    char *msg;
    asprintf(&msg, "Hello World from " MACSTR " (%s)", MAC2STR(node_mac), is_root ? "root" : "node");
    size_t len = strlen(msg);

    ESP_LOGI(TAG, "Starting MQTT client");

    // start
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    // wait for connection
    EventBits_t bits = xEventGroupWaitBits(my_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    assert((bits & MQTT_CONNECTED_BIT) != 0);

    while (true) {
        ESP_ERROR_CHECK(esp_mqtt_client_publish(client, "/helloworld", msg, len, 0, 0));
        send_info(client);
        vTaskDelay(pdMS_TO_TICKS(5000));
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
                .sta_config = &sta_config,
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
