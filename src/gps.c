#include "gps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "GPS";

// UART Configuration for GPS
#define GPS_UART_NUM        UART_NUM_1
#define GPS_TXD_PIN         GPIO_NUM_17
#define GPS_RXD_PIN         GPIO_NUM_16
#define GPS_UART_BUF_SIZE   1024
#define GPS_BAUD_RATE       9600

static gps_data_t current_gps_data = {0};

/**
 * Convert NMEA coordinate format to decimal degrees
 */
static float nmea_to_decimal(const char *coord, char direction) {
    if (coord == NULL || strlen(coord) < 4) return 0.0;
    
    char degrees_str[4] = {0};
    int deg_len = (strlen(coord) > 5 && coord[4] == '.') ? 2 : 3;
    
    strncpy(degrees_str, coord, deg_len);
    float degrees = atof(degrees_str);
    float minutes = atof(coord + deg_len);
    
    float decimal = degrees + (minutes / 60.0);
    
    if (direction == 'S' || direction == 'W') {
        decimal = -decimal;
    }
    
    return decimal;
}

/**
 * Verify NMEA checksum
 */
static bool verify_checksum(char *sentence) {
    char *checksum_str = strchr(sentence, '*');
    if (checksum_str == NULL) return false;
    
    uint8_t checksum = 0;
    for (char *p = sentence + 1; p < checksum_str; p++) {
        checksum ^= *p;
    }
    
    uint8_t expected = (uint8_t)strtol(checksum_str + 1, NULL, 16);
    return (checksum == expected);
}

/**
 * Parse GPGGA sentence
 */
static void parse_gpgga(char *sentence) {
    char *token;
    int field = 0;
    char lat_str[15] = {0}, lon_str[15] = {0};
    char lat_dir = 'N', lon_dir = 'E';
    
    token = strtok(sentence, ",");
    
    while (token != NULL && field < 15) {
        switch (field) {
            case 1: // Time
                if (strlen(token) >= 6) {
                    snprintf(current_gps_data.time, sizeof(current_gps_data.time), 
                            "%.2s:%.2s:%.2s", token, token+2, token+4);
                }
                break;
            case 2: // Latitude
                strncpy(lat_str, token, sizeof(lat_str) - 1);
                break;
            case 3: // N/S
                lat_dir = token[0];
                break;
            case 4: // Longitude
                strncpy(lon_str, token, sizeof(lon_str) - 1);
                break;
            case 5: // E/W
                lon_dir = token[0];
                break;
            case 6: // Fix quality
                current_gps_data.fix_quality = atoi(token);
                current_gps_data.valid = (current_gps_data.fix_quality > 0);
                break;
            case 7: // Number of satellites
                current_gps_data.satellites = atoi(token);
                break;
            case 9: // Altitude
                current_gps_data.altitude = atof(token);
                break;
        }
        field++;
        token = strtok(NULL, ",");
    }
    
    if (strlen(lat_str) > 0 && strlen(lon_str) > 0) {
        current_gps_data.latitude = nmea_to_decimal(lat_str, lat_dir);
        current_gps_data.longitude = nmea_to_decimal(lon_str, lon_dir);
    }
}

/**
 * Parse GPRMC sentence
 */
static void parse_gprmc(char *sentence) {
    char *token;
    int field = 0;
    char lat_str[15] = {0}, lon_str[15] = {0};
    char lat_dir = 'N', lon_dir = 'E';
    
    token = strtok(sentence, ",");
    
    while (token != NULL && field < 12) {
        switch (field) {
            case 1: // Time
                if (strlen(token) >= 6) {
                    snprintf(current_gps_data.time, sizeof(current_gps_data.time), 
                            "%.2s:%.2s:%.2s", token, token+2, token+4);
                }
                break;
            case 2: // Status (A=valid, V=invalid)
                current_gps_data.valid = (token[0] == 'A');
                break;
            case 3: // Latitude
                strncpy(lat_str, token, sizeof(lat_str) - 1);
                break;
            case 4: // N/S
                lat_dir = token[0];
                break;
            case 5: // Longitude
                strncpy(lon_str, token, sizeof(lon_str) - 1);
                break;
            case 6: // E/W
                lon_dir = token[0];
                break;
            case 9: // Date
                if (strlen(token) >= 6) {
                    snprintf(current_gps_data.date, sizeof(current_gps_data.date), 
                            "%.2s/%.2s/%.2s", token, token+2, token+4);
                }
                break;
        }
        field++;
        token = strtok(NULL, ",");
    }
    
    if (strlen(lat_str) > 0 && strlen(lon_str) > 0) {
        current_gps_data.latitude = nmea_to_decimal(lat_str, lat_dir);
        current_gps_data.longitude = nmea_to_decimal(lon_str, lon_dir);
    }
}

/**
 * Process NMEA sentence
 */
static void process_nmea_sentence(char *sentence) {
    if (sentence == NULL || sentence[0] != '$') return;
    
    if (!verify_checksum(sentence)) {
        ESP_LOGW(TAG, "Invalid checksum");
        return;
    }
    
    if (strncmp(sentence, "$GPGGA", 6) == 0) {
        parse_gpgga(sentence);
    } else if (strncmp(sentence, "$GPRMC", 6) == 0) {
        parse_gprmc(sentence);
    }
}

/**
 * GPS reading task
 */
void gps_task(void *pvParameters) {
    uint8_t *data = (uint8_t *)malloc(GPS_UART_BUF_SIZE);
    char sentence[128] = {0};
    int sentence_idx = 0;
    
    ESP_LOGI(TAG, "GPS task started");
    
    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, data, GPS_UART_BUF_SIZE, 100 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = data[i];
                
                if (c == '$') {
                    sentence_idx = 0;
                    sentence[sentence_idx++] = c;
                } else if (c == '\r' || c == '\n') {
                    if (sentence_idx > 0) {
                        sentence[sentence_idx] = '\0';
                        process_nmea_sentence(sentence);
                        sentence_idx = 0;
                    }
                } else if (sentence_idx < sizeof(sentence) - 1) {
                    sentence[sentence_idx++] = c;
                }
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    free(data);
}

/**
 * Initialize GPS UART
 */
void gps_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, GPS_UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TXD_PIN, GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "GPS initialized on UART%d (TX: GPIO%d, RX: GPIO%d)", 
             GPS_UART_NUM, GPS_TXD_PIN, GPS_RXD_PIN);
}

/**
 * Get current GPS data
 */
gps_data_t gps_get_data(void) {
    return current_gps_data;
}

/**
 * Check if GPS has valid fix
 */
bool gps_has_fix(void) {
    return current_gps_data.valid;
}