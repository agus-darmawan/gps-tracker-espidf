#include "mqtt_vehicle_client.h"
#include "mqtt_client.h"
#include "vehicle_performance.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "MQTT_VEHICLE";
static esp_mqtt_client_handle_t client = NULL;
static char vehicle_id[32] = {0};
static vehicle_state_t vehicle_state = {
    .is_active = false,
    .is_locked = true,
    .is_killed = false,
    .kill_scheduled = false
};

/**
 * Get current ISO8601 timestamp
 */
static void get_timestamp(char* buffer, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t now = tv.tv_sec;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S", &timeinfo);
    snprintf(buffer + strlen(buffer), size - strlen(buffer), ".%03ldZ", tv.tv_usec / 1000);
}

/**
 * Handle incoming MQTT messages (commands)
 */
static void handle_command(const char* topic, const char* data, int data_len) {
    char command[32] = {0};
    
    // Extract command from topic: control.{command}.{vehicle_id}
    const char* cmd_start = strstr(topic, "control.");
    if (cmd_start) {
        cmd_start += strlen("control.");
        const char* cmd_end = strchr(cmd_start, '.');
        if (cmd_end) {
            int len = cmd_end - cmd_start;
            if (len < sizeof(command)) {
                strncpy(command, cmd_start, len);
                command[len] = '\0';
            }
        }
    }
    
    ESP_LOGI(TAG, "Received command: %s", command);
    
    // Parse JSON payload
    cJSON *json = cJSON_ParseWithLength(data, data_len);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse JSON command");
        return;
    }
    
    // Handle commands
    if (strcmp(command, "start_rent") == 0) {
        cJSON *order_id_json = cJSON_GetObjectItem(json, "order_id");
        if (order_id_json && cJSON_IsString(order_id_json)) {
            strncpy(vehicle_state.order_id, order_id_json->valuestring, sizeof(vehicle_state.order_id) - 1);
            ESP_LOGI(TAG, "Starting rent with order_id: %s", vehicle_state.order_id);
        }
        
        vehicle_state.is_locked = false;
        vehicle_state.is_active = true;
        vehicle_state.is_killed = false;
        vehicle_state.kill_scheduled = false;
        
        // Start performance tracking
        performance_start_tracking(vehicle_state.order_id);
        
        ESP_LOGI(TAG, "Vehicle unlocked and activated");
    }
    else if (strcmp(command, "end_rent") == 0) {
        vehicle_state.is_active = false;
        vehicle_state.is_locked = true;
        
        // Stop performance tracking and send report
        performance_stop_tracking();
        mqtt_publish_performance();
        
        memset(vehicle_state.order_id, 0, sizeof(vehicle_state.order_id));
        ESP_LOGI(TAG, "Rent ended, vehicle locked");
    }
    else if (strcmp(command, "kill_vehicle") == 0) {
        vehicle_state.kill_scheduled = true;
        ESP_LOGW(TAG, "Kill vehicle scheduled (waiting for low speed)");
    }
    
    cJSON_Delete(json);
    
    // Publish status update
    mqtt_publish_status(vehicle_state.is_active, vehicle_state.is_locked, vehicle_state.is_killed);
}

/**
 * MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            
            // Subscribe to control commands
            char topic[128];
            snprintf(topic, sizeof(topic), "control.start_rent.%s", vehicle_id);
            esp_mqtt_client_subscribe(client, topic, 1);
            
            snprintf(topic, sizeof(topic), "control.end_rent.%s", vehicle_id);
            esp_mqtt_client_subscribe(client, topic, 1);
            
            snprintf(topic, sizeof(topic), "control.kill_vehicle.%s", vehicle_id);
            esp_mqtt_client_subscribe(client, topic, 1);
            
            ESP_LOGI(TAG, "Subscribed to control topics");
            
            // Send registration message
            mqtt_publish_registration();
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Message received on topic: %.*s", event->topic_len, event->topic);
            
            // Handle command
            char topic_buf[256];
            strncpy(topic_buf, event->topic, event->topic_len);
            topic_buf[event->topic_len] = '\0';
            
            handle_command(topic_buf, event->data, event->data_len);
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            break;
            
        default:
            break;
    }
}

/**
 * Initialize MQTT client
 */
void mqtt_vehicle_init(const char* vid) {
    if (vid != NULL) {
        strncpy(vehicle_id, vid, sizeof(vehicle_id) - 1);
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive = MQTT_KEEPALIVE,
        .network.disable_auto_reconnect = false,
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    ESP_LOGI(TAG, "MQTT client initialized for vehicle: %s", vehicle_id);
}

/**
 * Start MQTT client
 */
void mqtt_vehicle_start(void) {
    if (client) {
        esp_mqtt_client_start(client);
        ESP_LOGI(TAG, "MQTT client started");
    }
}

/**
 * Stop MQTT client
 */
void mqtt_vehicle_stop(void) {
    if (client) {
        esp_mqtt_client_stop(client);
        ESP_LOGI(TAG, "MQTT client stopped");
    }
}

/**
 * Check if MQTT is connected
 */
bool mqtt_vehicle_is_connected(void) {
    return (client != NULL);
}

/**
 * Publish location data
 */
void mqtt_publish_location(float latitude, float longitude, float altitude) {
    if (!client) return;
    
    char topic[128];
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "vehicle_id", vehicle_id);
    cJSON_AddNumberToObject(root, "latitude", latitude);
    cJSON_AddNumberToObject(root, "longitude", longitude);
    cJSON_AddNumberToObject(root, "altitude", altitude);
    cJSON_AddStringToObject(root, "timestamp", timestamp);
    
    char *payload = cJSON_PrintUnformatted(root);
    
    snprintf(topic, sizeof(topic), "realtime.location.%s", vehicle_id);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    
    ESP_LOGD(TAG, "Published location: %.6f, %.6f", latitude, longitude);
    
    cJSON_free(payload);
    cJSON_Delete(root);
}

/**
 * Publish status data
 */
void mqtt_publish_status(bool is_active, bool is_locked, bool is_killed) {
    if (!client) return;
    
    char topic[128];
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "vehicle_id", vehicle_id);
    cJSON_AddBoolToObject(root, "is_active", is_active);
    cJSON_AddBoolToObject(root, "is_locked", is_locked);
    cJSON_AddBoolToObject(root, "is_killed", is_killed);
    cJSON_AddStringToObject(root, "timestamp", timestamp);
    
    char *payload = cJSON_PrintUnformatted(root);
    
    snprintf(topic, sizeof(topic), "realtime.status.%s", vehicle_id);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    
    ESP_LOGD(TAG, "Published status: active=%d, locked=%d", is_active, is_locked);
    
    cJSON_free(payload);
    cJSON_Delete(root);
}

/**
 * Publish battery data
 */
void mqtt_publish_battery(float voltage, float battery_level) {
    if (!client) return;
    
    char topic[128];
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "vehicle_id", vehicle_id);
    cJSON_AddNumberToObject(root, "device_voltage", voltage);
    cJSON_AddNumberToObject(root, "device_battery_level", battery_level);
    cJSON_AddStringToObject(root, "timestamp", timestamp);
    
    char *payload = cJSON_PrintUnformatted(root);
    
    snprintf(topic, sizeof(topic), "realtime.battery.%s", vehicle_id);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    
    ESP_LOGD(TAG, "Published battery: %.2fV, %.2f%%", voltage, battery_level);
    
    cJSON_free(payload);
    cJSON_Delete(root);
}

/**
 * Publish performance report
 */
void mqtt_publish_performance(void) {
    if (!client) return;
    
    vehicle_performance_t perf = performance_get_data();
    
    char topic[128];
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "vehicle_id", vehicle_id);
    cJSON_AddStringToObject(root, "order_id", perf.order_id);
    cJSON_AddStringToObject(root, "weight_score", perf.weight_score);
    cJSON_AddNumberToObject(root, "front_tire", perf.s_front_tire);
    cJSON_AddNumberToObject(root, "rear_tire", perf.s_rear_tire);
    cJSON_AddNumberToObject(root, "brake_pad", perf.s_brake_pad);
    cJSON_AddNumberToObject(root, "engine_oil", perf.s_engine_oil);
    cJSON_AddNumberToObject(root, "chain_or_cvt", perf.s_chain_or_cvt);
    cJSON_AddNumberToObject(root, "engine", perf.s_engine);
    cJSON_AddNumberToObject(root, "distance_travelled", perf.total_distance_km);
    cJSON_AddNumberToObject(root, "average_speed", perf.average_speed);
    cJSON_AddNumberToObject(root, "max_speed", perf.max_speed);
    cJSON_AddStringToObject(root, "timestamp", timestamp);
    
    char *payload = cJSON_PrintUnformatted(root);
    
    snprintf(topic, sizeof(topic), "report.performance.%s", vehicle_id);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    
    ESP_LOGI(TAG, "Published performance report for order: %s", perf.order_id);
    
    cJSON_free(payload);
    cJSON_Delete(root);
}

/**
 * Publish vehicle registration
 */
void mqtt_publish_registration(void) {
    if (!client) return;
    
    char topic[128];
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "vehicle_id", vehicle_id);
    
    char *payload = cJSON_PrintUnformatted(root);
    
    snprintf(topic, sizeof(topic), "registration.new");
    esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    
    ESP_LOGI(TAG, "Published registration for vehicle: %s", vehicle_id);
    
    cJSON_free(payload);
    cJSON_Delete(root);
}

/**
 * Get vehicle state
 */
vehicle_state_t* mqtt_get_vehicle_state(void) {
    return &vehicle_state;
}