#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "max6675.h"

static const char *TAG = "MAX6675";

/**
 * Main application task.
 * Initializes the MAX6675 sensor and periodically reads temperature.
 */
void app_main(void) {
    esp_err_t ret;

    // Initialize MAX6675 sensor
    ret = max6675_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAX6675 initialization failed");
        return;
    }

    while (1) {
        // Read and print temperature
        float temperature = max6675_read_temperature();
        if (temperature != -1.0f) {
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        }

        // Wait for 1 second before reading again
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
