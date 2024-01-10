#include "demo_mqtt.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>
#include <sdkconfig.h>

#include "image.h"

#define SNTP_SYNCED_BIT BIT0

static const char* TAG = "demo_mqtt";

static EventGroupHandle_t sntp_event_group;

static const int user_id = 40;
static const int device_id = 98;
static const int sensor_id = 282;
static const char* sensor_name = "MeshNOW Fake Camera";
static const char* field_name = "image_name";
static const char* filename = "image";
static const char* fileending = "jpg";

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void sntp_sync_cb(struct timeval* tv) {
    ESP_LOGI(TAG, "SNTP sync complete!");
    // set timezone to Berlin
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    // print current time
    struct tm timeinfo;
    localtime_r(&tv->tv_sec, &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", asctime(&timeinfo));
    xEventGroupSetBits(sntp_event_group, SNTP_SYNCED_BIT);
    ESP_LOGI(TAG, "Sync done!");
}

static int64_t get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
}

static void send_filedescriptor(const char* file) {
    ESP_LOGI(TAG, "Sending file descriptor for: %s", file);

    char* topic;
    asprintf(&topic, "%d/%d/data", user_id, device_id);

    int64_t time = get_timestamp();
    char* payload;
    // {"sensors":[{"name":"sensor_name","values":[{"timestamp":get_timestamp(),"image":{"fileId":"file"}}]}]}
    int bytes = asprintf(
        &payload, "{\"sensors\":[{\"name\":\"%s\",\"values\":[{\"timestamp\":%lld,\"image\":{\"fileId\":\"%s\"}}]}]}",
        sensor_name, time, file);

    ESP_LOGI(TAG, "Publishing: %s", payload);
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, bytes, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish filedescriptor!");
        abort();
    }

    free(payload);
    free(topic);
}

static void send_data(const char* file) {
    ESP_LOGI(TAG, "Sending image data for: %s", file);

    char* topic;
    asprintf(&topic, "%d/%d/%d/%s/%s", user_id, device_id, sensor_id, field_name, file);

    int remaining_size = image_jpg_len;
    int chunk_num = 0;
    int max_chunks = (image_jpg_len + 1) / 1024 + 1;

    while (remaining_size > 0) {
        int chunk_size = remaining_size > 1024 ? 1024 : remaining_size;
        ESP_LOGI(TAG, "Chunk %d/%d", chunk_num++, max_chunks);
        int msg_id =
            esp_mqtt_client_publish(mqtt_client, topic, image_jpg + image_jpg_len - remaining_size, chunk_size, 0, 0);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Failed to publish image data!");
            abort();
        }
        remaining_size -= chunk_size;
    }

    ESP_LOGI(TAG, "Closing bytestream");
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, "", 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish image data!");
        abort();
    }

    ESP_LOGI(TAG, "DONE");

    free(topic);
}

void start_mqtt(void) {
    // Synchronize SNTP time
    ESP_LOGI(TAG, "Configuring SNTP...");
    // create event group
    sntp_event_group = xEventGroupCreate();
    assert(sntp_event_group != NULL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(&sntp_sync_cb);
    sntp_init();
    ESP_LOGI(TAG, "Waiting for SNTP sync...");

    ESP_LOGI(TAG, "Starting MQTT client...");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .network.reconnect_timeout_ms = 3 * 1000,
        .network.timeout_ms = 60 * 1000,
        .credentials =
            {
                .username = CONFIG_MQTT_USERNAME,
                .authentication.password = CONFIG_MQTT_PASSWORD,
            },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    assert(mqtt_client != NULL);
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    ESP_LOGI(TAG, "MQTT client started!");

    // we now need to wait for the SNTP sync to complete
    xEventGroupWaitBits(sntp_event_group, SNTP_SYNCED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Sending image...");
    // measure time it takes in milliseconds
    int64_t start = esp_timer_get_time();
    char* timestamped_filename;
    asprintf(&timestamped_filename, "%s-%lld.%s", filename, get_timestamp(), fileending);
    send_filedescriptor(timestamped_filename);
    send_data(timestamped_filename);
    free(timestamped_filename);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "Image sent successfully");
    ESP_LOGI(TAG, "Time taken: %lldus", end - start);
}