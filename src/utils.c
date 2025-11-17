#include "utils.h"
#include "esp_log.h"

/**
 * Print startup banner
 */

#define TAG_MAIN "MAIN"
static void print_banner(void) {
    ESP_LOGI(TAG_MAIN, "");
    ESP_LOGI(TAG_MAIN, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG_MAIN, "║                                                    ║");
    ESP_LOGI(TAG_MAIN, "║          Vehicle GPS Tracker v1.0.0                ║");
    ESP_LOGI(TAG_MAIN, "║          ESP32 IoT Device                          ║");
    ESP_LOGI(TAG_MAIN, "║                                                    ║");
    ESP_LOGI(TAG_MAIN, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG_MAIN, "");
}

/**
 * Print system ready message
 */
static void print_ready_message(void) {
    ESP_LOGI(TAG_MAIN, "");
    ESP_LOGI(TAG_MAIN, "╔════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG_MAIN, "║           SYSTEM READY & OPERATIONAL               ║");
    ESP_LOGI(TAG_MAIN, "╠════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG_MAIN, "║  Vehicle ID: %-38s║", web_config_get_vehicle_id());
    ESP_LOGI(TAG_MAIN, "║  Status: ACTIVE                                    ║");
    ESP_LOGI(TAG_MAIN, "║  Mode: PRODUCTION                                  ║");
    ESP_LOGI(TAG_MAIN, "╚════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG_MAIN, "");
}