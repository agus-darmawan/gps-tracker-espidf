#include "max6675.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing MAX6675...");
    esp_err_t ret = max6675_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAX6675 init failed: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        float temp = max6675_read_temperature();
        if (temp < 0) {
            ESP_LOGW(TAG, "Read failed or thermocouple disconnected!");
        } else {
            ESP_LOGI(TAG, "Temperature: %.2f °C", temp);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
