#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Project modules
#include "wifi_manager.h"
#include "max6675.h"
#include "mpu6050.h"
#include "vehicle_performance.h"
#include "utils.h"
#include "mqtt_vehicle_client.h"

#define TAG "MAIN"

// WiFi credentials (Change these for your network)
#define WIFI_SSID "darmawan"
#define WIFI_PASS "password"

const char* mode = "DEBUG"; // Set mode to "DEBUG" or "PRODUCTION"

// functions forward declarations
static bool initialize_wifi(void);
static esp_err_t initialize_sim808(void);
static void initialize_sensors(void);
static void initialize_performance(void);


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
 * Main application entry point
 */
void app_main(void) {
    print_banner();
    
    ESP_LOGI(TAG, "Starting system initialization...");
    ESP_LOGI(TAG, "");
    
    if(mode == "DEBUG"){
        ESP_LOGI(TAG, "Mode: DEBUG");
        ESP_LOGI(TAG, " Connecting to WiFi...");
        if (!initialize_wifi()) {
            ESP_LOGE(TAG, "WiFi connection failed. Restarting in 10 seconds...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            esp_restart();
        }
    } else {
        ESP_LOGI(TAG, "Mode: PRODUCTION");
        ESP_LOGI(TAG, "[2/7] Initializing SIM808 module...");
        if (initialize_sim808() != ESP_OK) {
            ESP_LOGE(TAG, "SIM808 initialization failed. Restarting in 10 seconds...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            esp_restart();
        }
    }


    ESP_LOGI(TAG, " Initializing sensors...");
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

static esp_err_t initialize_sim808(void) {
    ESP_LOGI(TAG, "Initializing SIM808...");
    
    // Initialize UART
    if (sim808_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SIM808 UART");
        return ESP_FAIL;
    }
    
    // Power on module
    if (sim808_power_on() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on SIM808");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ SIM808 module powered on");
    
    // Wait for network registration
    ESP_LOGI(TAG, "Waiting for network registration...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    sim808_get_network_status();
    
    // Check signal quality
    int rssi, ber;
    if (sim808_get_signal_quality(&rssi, &ber) == ESP_OK) {
        ESP_LOGI(TAG, "✓ Signal quality: RSSI=%d, BER=%d", rssi, ber);
    }
    
    return ESP_OK;
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

    if(gps_init() != ESP_OK){
        ESP_LOGW(TAG, "⚠ GPS initialization failed, continuing without GPS");
    } else {
        ESP_LOGI(TAG, "✓ GPS initialized");
    }
    ESP_LOGI(TAG, "Sensor initialization complete");
}

/**
 * Initialize GPS
 */
static esp_err_t gps_init(void) {
    ESP_LOGI(TAG, "Initializing GPS...");
    
    if (sim808_gps_power_on() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on GPS");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ GPS powered on, waiting for fix...");
    
    // Wait for GPS fix
    int retry = 0;
    while (retry < 30) {  // Wait max 1 minute
        if (sim808_gps_has_fix()) {
            ESP_LOGI(TAG, "✓ GPS fix acquired!");
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Waiting for GPS fix... (%d/30)", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry++;
    }
    
    ESP_LOGW(TAG, "GPS fix not acquired yet, continuing anyway...");
    return ESP_OK;
}


/**

 * Connect to GPRS network
 */
static esp_err_t connect_gprs(void) {
    ESP_LOGI(TAG, "Connecting to GPRS network...");
    
    sim808_gprs_config_t gprs_config = {
        .apn = GPRS_APN,
        .username = GPRS_USER,
        .password = GPRS_PASS,
    };
    
    if (sim808_gprs_connect(&gprs_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to GPRS");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ GPRS connected successfully");
    return ESP_OK;
}

/**
 * Connect to MQTT broker (RabbitMQ)
 */
static esp_err_t connect_mqtt(void) {
    ESP_LOGI(TAG, "Connecting to MQTT broker (RabbitMQ)...");
    
    sim808_mqtt_config_t mqtt_config = {
        .broker = MQTT_BROKER,
        .port = MQTT_PORT,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .client_id = VEHICLE_ID,
    };
    
    if (sim808_mqtt_connect(&mqtt_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to MQTT");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ MQTT connected to RabbitMQ");
    
    // Subscribe to control topics
    char topic[128];
    snprintf(topic, sizeof(topic), "control.start_rent.%s", VEHICLE_ID);
    sim808_mqtt_subscribe(topic);
    
    snprintf(topic, sizeof(topic), "control.end_rent.%s", VEHICLE_ID);
    sim808_mqtt_subscribe(topic);
    
    // Publish registration
    vTaskDelay(pdMS_TO_TICKS(1000));
    publish_registration();
    
    return ESP_OK;
}
