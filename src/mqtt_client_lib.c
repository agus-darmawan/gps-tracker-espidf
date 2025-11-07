#include "mqtt_client_lib.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MQTT_LIB";
static esp_mqtt_client_handle_t client = NULL;
static mqtt_message_callback_t user_callback = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT Broker");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT Broker");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Message received on topic: %.*s", event->topic_len, event->topic);
        if (user_callback)
        {
            char topic[128];
            char payload[256];
            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            snprintf(payload, sizeof(payload), "%.*s", event->data_len, event->data);
            user_callback(topic, payload);
        }
        break;

    default:
        break;
    }
}

void mqtt_client_init(const char *broker_uri)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "MQTT client initialized with broker: %s", broker_uri);
}

void mqtt_publish(const char *topic, const char *payload)
{
    if (client)
    {
        esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published to [%s]: %s", topic, payload);
    }
}

void mqtt_subscribe(const char *topic, mqtt_message_callback_t callback)
{
    if (client)
    {
        esp_mqtt_client_subscribe(client, topic, 1);
        user_callback = callback;
        ESP_LOGI(TAG, "Subscribed to topic: %s", topic);
    }
}
