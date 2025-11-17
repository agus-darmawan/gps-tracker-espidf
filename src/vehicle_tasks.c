#include "vehicle_tasks.h"
#include "max6675.h"
#include "mpu6050.h"
#include "vehicle_performance.h"
#include "mqtt_vehicle_client.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "VEHICLE_TASKS";

// Task handles
TaskHandle_t gps_task_handle = NULL;
TaskHandle_t tracking_task_handle = NULL;
TaskHandle_t monitor_task_handle = NULL;

// Update intervals (in milliseconds)
#define GPS_UPDATE_INTERVAL     5000    // 5 seconds
#define STATUS_UPDATE_INTERVAL  5000    // 5 seconds
#define BATTERY_UPDATE_INTERVAL 10000   // 10 seconds
#define TEMP_CHECK_INTERVAL     5000    // 5 seconds

// Battery simulation (TODO: replace with real ADC reading)
static float battery_voltage = 12.6;
static float battery_level = 100.0;

/**
 * Calculate distance between two GPS coordinates using Haversine formula
 */
static float calculate_distance(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371000; // Earth radius in meters
    float lat1_rad = lat1 * M_PI / 180.0;
    float lat2_rad = lat2 * M_PI / 180.0;
    float dlat = (lat2 - lat1) * M_PI / 180.0;
    float dlon = (lon2 - lon1) * M_PI / 180.0;
    
    float a = sin(dlat/2) * sin(dlat/2) +
              cos(lat1_rad) * cos(lat2_rad) *
              sin(dlon/2) * sin(dlon/2);
    float c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return R * c;
}

/**
 * Calculate speed from distance and time
 */
static float calculate_speed(float distance, float time_diff) {
    if (time_diff <= 0) return 0;
    return (distance / time_diff) * 3.6; // Convert m/s to km/h
}


/**
 * Main vehicle tracking task
 * Handles GPS updates, sensor readings, and MQTT publishing
 */
void vehicle_tracking_task(void *pvParameters) {
    gps_data_t last_gps = {0};
    TickType_t last_gps_time = 0;
    TickType_t last_status_time = 0;
    TickType_t last_battery_time = 0;
    TickType_t last_temp_check = 0;
    
    bool gps_initialized = false;
    float last_speed = 0;
    float engine_temp = 0;
    
    ESP_LOGI(TAG, "Vehicle tracking task started");
    
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        vehicle_state_t *state = mqtt_get_vehicle_state();
        
        // GPS Update
        if ((current_time - last_gps_time) >= pdMS_TO_TICKS(GPS_UPDATE_INTERVAL)) {
            gps_data_t current_gps = gps_get_data();
            
            if (current_gps.valid) {
                // Publish location
                mqtt_publish_location(current_gps.latitude, current_gps.longitude, current_gps.altitude);
                
                // Calculate performance if tracking is active
                if (gps_initialized && state->is_active) {
                    float distance = calculate_distance(
                        last_gps.latitude, last_gps.longitude,
                        current_gps.latitude, current_gps.longitude
                    );
                    
                    float time_diff = (current_time - last_gps_time) * portTICK_PERIOD_MS / 1000.0;
                    float speed = calculate_speed(distance, time_diff);
                    
                    // Get pitch angle from MPU6050 to estimate elevation change
                    mpu6050_data_t mpu_data;
                    int elevation_change = 0;
                    if (mpu6050_read_data(&mpu_data) == ESP_OK) {
                        // Approximate elevation change from pitch angle and distance
                        elevation_change = (int)(distance * sin(mpu_data.pitch * M_PI / 180.0));
                    }
                    
                    // Update performance calculator
                    performance_update(
                        (int)distance,                    // s_real
                        elevation_change,                 // h
                        (int)speed,                       // v_end
                        engine_temp                       // T_machine
                    );
                    
                    last_speed = speed;
                    
                    // Check if kill is scheduled and speed is low enough
                    if (state->kill_scheduled && speed < 10) {
                        state->is_active = false;
                        state->is_locked = true;
                        state->is_killed = true;
                        state->kill_scheduled = false;
                        ESP_LOGW(TAG, "Vehicle killed (speed < 10 km/h)");
                        mqtt_publish_status(state->is_active, state->is_locked, state->is_killed);
                    }
                }
                
                last_gps = current_gps;
                gps_initialized = true;
            } else {
                if (!gps_initialized) {
                    ESP_LOGD(TAG, "Waiting for GPS fix...");
                }
            }
            
            last_gps_time = current_time;
        }
        
        // Status Update
        if ((current_time - last_status_time) >= pdMS_TO_TICKS(STATUS_UPDATE_INTERVAL)) {
            mqtt_publish_status(state->is_active, state->is_locked, state->is_killed);
            last_status_time = current_time;
        }
        
        // Battery Update
        if ((current_time - last_battery_time) >= pdMS_TO_TICKS(BATTERY_UPDATE_INTERVAL)) {
            // Simulate battery drain when active (TODO: implement real ADC reading)
            if (state->is_active) {
                battery_level -= 0.1;
                if (battery_level < 0) battery_level = 0;
                battery_voltage = 10.5 + (battery_level / 100.0) * 2.1;
            }
            
            mqtt_publish_battery(battery_voltage, battery_level);
            last_battery_time = current_time;
        }
        
        // Temperature Check
        if ((current_time - last_temp_check) >= pdMS_TO_TICKS(TEMP_CHECK_INTERVAL)) {
            engine_temp = max6675_read_temperature();
            if (engine_temp < 0) {
                engine_temp = 85.0; // Default temperature if sensor fails
            }
            ESP_LOGD(TAG, "Engine temperature: %.2f°C", engine_temp);
            last_temp_check = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

/**
 * System monitor task
 * Monitors system health and logs periodic status
 */
void system_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor task started");
    
    TickType_t last_log_time = 0;
    const TickType_t log_interval = pdMS_TO_TICKS(60000); // Log every minute
    
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        
        if ((current_time - last_log_time) >= log_interval) {
            // Log system health
            ESP_LOGI(TAG, "=== System Status ===");
            ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
            ESP_LOGI(TAG, "Min free heap: %lu bytes", esp_get_minimum_free_heap_size());
            ESP_LOGI(TAG, "Vehicle ID: %s", web_config_get_vehicle_id());
            ESP_LOGI(TAG, "MQTT connected: %s", mqtt_vehicle_is_connected() ? "Yes" : "No");
            ESP_LOGI(TAG, "GPS fix: %s", gps_has_fix() ? "Yes" : "No");
            ESP_LOGI(TAG, "====================");
            
            last_log_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Check every 10 seconds
    }
    
    vTaskDelete(NULL);
}

/**
 * Initialize vehicle tasks
 */
void vehicle_tasks_init(void) {
    ESP_LOGI(TAG, "Initializing vehicle tasks");
    
    // Reset task handles
    gps_task_handle = NULL;
    tracking_task_handle = NULL;
    monitor_task_handle = NULL;
}

/**
 * Start all vehicle tasks
 */
void vehicle_tasks_start(void) {
    ESP_LOGI(TAG, "Starting vehicle tasks");
    
    // Create GPS reading task
    BaseType_t ret = xTaskCreate(
        gps_task,
        "gps_task",
        GPS_TASK_STACK_SIZE,
        NULL,
        GPS_TASK_PRIORITY,
        &gps_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create GPS task");
    } else {
        ESP_LOGI(TAG, "GPS task created");
    }
    
    // Create main tracking task
    ret = xTaskCreate(
        vehicle_tracking_task,
        "tracking_task",
        TRACKING_TASK_STACK_SIZE,
        NULL,
        TRACKING_TASK_PRIORITY,
        &tracking_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create tracking task");
    } else {
        ESP_LOGI(TAG, "Tracking task created");
    }
    
    // Create system monitor task
    ret = xTaskCreate(
        system_monitor_task,
        "monitor_task",
        MONITOR_TASK_STACK_SIZE,
        NULL,
        MONITOR_TASK_PRIORITY,
        &monitor_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
    } else {
        ESP_LOGI(TAG, "Monitor task created");
    }
}

/**
 * Stop all vehicle tasks
 */
void vehicle_tasks_stop(void) {
    ESP_LOGI(TAG, "Stopping vehicle tasks");
    
    if (gps_task_handle != NULL) {
        vTaskDelete(gps_task_handle);
        gps_task_handle = NULL;
    }
    
    if (tracking_task_handle != NULL) {
        vTaskDelete(tracking_task_handle);
        tracking_task_handle = NULL;
    }
    
    if (monitor_task_handle != NULL) {
        vTaskDelete(monitor_task_handle);
        monitor_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "All vehicle tasks stopped");
}