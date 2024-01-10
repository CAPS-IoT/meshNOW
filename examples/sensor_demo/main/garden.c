#include "garden.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <sys/cdefs.h>

#include "bh1750.h"
#include "dht.h"
#include "i2cdev.h"

#define BH1750_I2C I2C_NUM_1
#define BH1750_SCL GPIO_NUM_22
#define BH1750_SDA GPIO_NUM_21
#define BH1750_ADDR BH1750_ADDR_LO

#define DHT11_TYPE DHT_TYPE_DHT11
#define DHT11_GPIO GPIO_NUM_25

#define MQTT_CONNECT_BIT BIT0

static const char* TAG = "garden";

// random topic
static const char* TOPIC = "bzasgdjksajbdhjagsdzat6dgauzdjbasdbasdh";

static EventGroupHandle_t wait_bits;

static esp_mqtt_client_handle_t client_handle;

static QueueHandle_t measure_queue;

static i2c_dev_t* bh1750_dev;

typedef struct {
    float temperature;
    float humidity;
    uint16_t lux;
} sensor_data_t;

_Noreturn static void measure_task(void* pvParameters) {
    sensor_data_t data;

    while (true) {
        // reset
        data.temperature = 0;
        data.humidity = 0;
        data.lux = 0;

        // Read Lux
        if (bh1750_read(bh1750_dev, &data.lux) != ESP_OK) {
            ESP_LOGE(TAG, "Could not read data from BH1750 sensor");
        }

        // Read temperature and humidity
        if (dht_read_float_data(DHT11_TYPE, DHT11_GPIO, &data.humidity, &data.temperature) != ESP_OK) {
            ESP_LOGE(TAG, "Could not read data from DHT11 sensor");
        }

        ESP_LOGI(TAG, "Temperature: %.1f°C | Humidity: %.1f%% | Luminance: %dlx", data.temperature, data.humidity,
                 data.lux);

        // send data to queue
        xQueueSend(measure_queue, &data, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(wait_bits, MQTT_CONNECT_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            abort();
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

_Noreturn static void publish_task(void* pvParameters) {
    // await connection
    xEventGroupWaitBits(wait_bits, MQTT_CONNECT_BIT, true, true, portMAX_DELAY);
    ESP_LOGI(TAG, "MQTT connected");

    while (true) {
        sensor_data_t data;
        if (xQueueReceive(measure_queue, &data, portMAX_DELAY) == pdTRUE) {
            char* text;
            // as json
            int size = asprintf(&text, "{\"temperature\":%.1f,\"humidity\":%.1f,\"lux\":%d}", data.temperature,
                                data.humidity, data.lux);
            ESP_LOGI(TAG, "Publishing: %s", text);
            esp_mqtt_client_publish(client_handle, TOPIC, text, size, 1, 0);
            free(text);
        }
    }
}

static void init() {
    // init i2cdev
    ESP_ERROR_CHECK(i2cdev_init());

    // init bh1750
    bh1750_dev = calloc(1, sizeof(i2c_dev_t));
    assert(bh1750_dev != NULL);
    ESP_ERROR_CHECK(bh1750_init_desc(bh1750_dev, BH1750_ADDR, BH1750_I2C, BH1750_SDA, BH1750_SCL));
    ESP_ERROR_CHECK(bh1750_setup(bh1750_dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));

    // init mqtt
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_EXAMPLE_MQTT_BROKER_URI,
    };

    client_handle = esp_mqtt_client_init(&mqtt_cfg);
    assert(client_handle != NULL);
    // create event bits
    wait_bits = xEventGroupCreate();
    assert(wait_bits != NULL);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, client_handle));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client_handle));
}

void perform_garden() {
    init();

    // create queue
    measure_queue = xQueueCreate(10, sizeof(sensor_data_t));
    assert(measure_queue != NULL);

    // create measurement task
    assert(xTaskCreate(measure_task, "measure_task", 2048, NULL, 5, NULL) == pdPASS);

    // create mqtt publish task
    assert(xTaskCreate(publish_task, "publish_task", 2048, NULL, 5, NULL) == pdPASS);
}