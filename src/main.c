#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Project modules
#include "wifi_manager.h"
#include "gps.h"
#include "max6675.h"
#include "mpu6050.h"
#include "vehicle_performance.h"
#include "mqtt_vehicle_client.h"

#define TAG "MAIN"

// WiFi credentials (Change these for your network)
#define WIFI_SSID "darmawan"
#define WIFI_PASS "password"

const char* mode = "DEBUG"; // Set mode to "DEBUG" or "PRODUCTION"


/**
 * Initialize and connect to WiFi
 */
static bool initialize_wifi(void) {
    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
    wifi_init_sta(WIFI_SSID, WIFI_PASS);
    
    // Wait for WiFi connection (max 30 seconds)
    int retry = 0;
    while (!wifi_is_connected() && retry < 30) {
        ESP_LOGI(TAG, "Waiting for WiFi connection... (%d/30)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }
    
    if (!wifi_is_connected()) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi connected successfully");
    return true;
}

/**
 * Initialize all sensors
 */
static void initialize_sensors(void) {
    ESP_LOGI(TAG, "Initializing sensors...");
    
    // Initialize GPS
    gps_init();
    ESP_LOGI(TAG, "✓ GPS initialized");
    
    // Initialize MAX6675 (temperature sensor)
    if (max6675_init() != ESP_OK) {
        ESP_LOGW(TAG, "⚠ MAX6675 initialization failed, continuing without temperature sensor");
    } else {
        ESP_LOGI(TAG, "✓ MAX6675 (temperature sensor) initialized");
    }
    
    // Initialize MPU6050 (IMU)
    if (mpu6050_init() != ESP_OK) {
        ESP_LOGW(TAG, "⚠ MPU6050 initialization failed, continuing without IMU");
    } else {
        ESP_LOGI(TAG, "✓ MPU6050 (IMU) initialized");
    }
    
    ESP_LOGI(TAG, "Sensor initialization complete");
}

/**
 * Initialize performance tracking system
 */
static void initialize_performance(void) {
    performance_init();
    ESP_LOGI(TAG, "✓ Performance tracking initialized");
}

/**
 * Initialize and start MQTT client
 */
static void initialize_mqtt(void) {
    mqtt_vehicle_init(web_config_get_vehicle_id());
    mqtt_vehicle_start();
    ESP_LOGI(TAG, "✓ MQTT client started");
    
    // Wait for MQTT connection
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * Start all vehicle tasks
 */
static void start_vehicle_tasks(void) {
    vehicle_tasks_init();
    vehicle_tasks_start();
    ESP_LOGI(TAG, "✓ Vehicle tasks started");
}

/**
 * Print startup banner
 */
static void print_banner(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                                                    ║");
    ESP_LOGI(TAG, "║          Vehicle GPS Tracker v1.0.0                ║");
    ESP_LOGI(TAG, "║          ESP32 IoT Device                          ║");
    ESP_LOGI(TAG, "║                                                    ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}

/**
 * Print system ready message
 */
static void print_ready_message(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║           SYSTEM READY & OPERATIONAL               ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  Vehicle ID: %-38s║", web_config_get_vehicle_id());
    ESP_LOGI(TAG, "║  Status: ACTIVE                                    ║");
    ESP_LOGI(TAG, "║  Mode: PRODUCTION                                  ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
}

/**
 * Main application entry point
 */
void app_main(void) {
    print_banner();
    
    ESP_LOGI(TAG, "Starting system initialization...");
    ESP_LOGI(TAG, "");
    
    // Step 1: Initialize NVS
    ESP_LOGI(TAG, "[1/8] Initializing NVS...");
    initialize_nvs();
    
    // Step 2: Connect to WiFi
    ESP_LOGI(TAG, "[2/8] Connecting to WiFi...");
    if (!initialize_wifi()) {
        ESP_LOGE(TAG, "WiFi connection failed. Restarting in 10 seconds...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    }
    
    // Step 3: Handle configuration
    ESP_LOGI(TAG, "[3/8] Checking device configuration...");
    handle_configuration();
    
    // Step 4: Initialize sensors
    ESP_LOGI(TAG, "[4/8] Initializing sensors...");
    initialize_sensors();
    
    // Step 5: Initialize performance tracking
    ESP_LOGI(TAG, "[5/8] Initializing performance tracking...");
    initialize_performance();
    
    // Step 6: Initialize MQTT
    ESP_LOGI(TAG, "[6/8] Initializing MQTT client...");
    initialize_mqtt();
    
    // Step 7: Start vehicle tasks
    ESP_LOGI(TAG, "[7/8] Starting vehicle tasks...");
    start_vehicle_tasks();
    
    // Step 8: System ready
    ESP_LOGI(TAG, "[8/8] System initialization complete!");
    ESP_LOGI(TAG, "");
    
    print_ready_message();
    
    // Main loop - just monitor system health
    while (1) {
        // Main task can do periodic checks here if needed
        vTaskDelay(pdMS_TO_TICKS(60000)); // Sleep for 1 minute
    }
}