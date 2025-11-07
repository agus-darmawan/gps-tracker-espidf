
#include "wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define WIFI_SSID "darmawan"
#define WIFI_PASS "password"


void app_main(void)
{
    // Initialize NVS (needed for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Connect to Wi-Fi
    wifi_init_sta(WIFI_SSID, WIFI_PASS);
}
