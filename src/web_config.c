#include "web_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WEB_CONFIG";
static vehicle_config_t config = {0};

/**
 * Initialize web configuration system
 */
void web_config_init(void) {
    memset(&config, 0, sizeof(vehicle_config_t));
    config.is_configured = false;
    ESP_LOGI(TAG, "Configuration system initialized");
}

/**
 * Load configuration from NVS
 */
void web_config_load(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    
    if (err == ESP_OK) {
        size_t required_size = sizeof(config.vehicle_id);
        err = nvs_get_str(nvs_handle, "vehicle_id", config.vehicle_id, &required_size);
        
        if (err == ESP_OK && strlen(config.vehicle_id) > 0) {
            config.is_configured = true;
            ESP_LOGI(TAG, "Loaded vehicle ID from NVS: %s", config.vehicle_id);
        } else {
            config.is_configured = false;
            ESP_LOGI(TAG, "No configuration found in NVS");
        }
        
        nvs_close(nvs_handle);
    } else {
        config.is_configured = false;
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
}

/**
 * Save configuration to NVS
 */
void web_config_save(const char* vehicle_id) {
    if (vehicle_id == NULL || strlen(vehicle_id) == 0) {
        ESP_LOGE(TAG, "Invalid vehicle ID");
        return;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, "vehicle_id", vehicle_id);
        
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
            
            if (err == ESP_OK) {
                strncpy(config.vehicle_id, vehicle_id, sizeof(config.vehicle_id) - 1);
                config.vehicle_id[sizeof(config.vehicle_id) - 1] = '\0';
                config.is_configured = true;
                
                ESP_LOGI(TAG, "Configuration saved to NVS: %s", vehicle_id);
            } else {
                ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "Failed to write to NVS: %s", esp_err_to_name(err));
        }
        
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
    }
}

/**
 * Check if device is configured
 */
bool web_config_is_configured(void) {
    return config.is_configured;
}

/**
 * Get configured vehicle ID
 */
const char* web_config_get_vehicle_id(void) {
    return config.vehicle_id;
}

/**
 * Get configuration structure
 */
vehicle_config_t* web_config_get(void) {
    return &config;
}