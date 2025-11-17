#ifndef SIM808_H
#define SIM808_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// UART Configuration for SIM808
#define SIM808_UART_NUM         UART_NUM_2
#define SIM808_TX_PIN           GPIO_NUM_17
#define SIM808_RX_PIN           GPIO_NUM_16
#define SIM808_BAUD_RATE        9600
#define SIM808_BUF_SIZE         1024

// SIM808 Control Pins
#define SIM808_POWER_PIN        GPIO_NUM_4
#define SIM808_RST_PIN          GPIO_NUM_2

// GPS Data Structure
typedef struct {
    bool valid;              // GPS fix status
    float latitude;          // Latitude in degrees (negative = South)
    float longitude;         // Longitude in degrees (negative = West)
    float altitude;          // Altitude in meters
    float speed;             // Speed in km/h
    int satellites;          // Number of satellites in view
    char timestamp[20];      // ISO8601 timestamp (YYYY-MM-DDTHH:MM:SSZ)
    char date[11];           // Date (YYYY-MM-DD)
} sim808_gps_data_t;

// GPRS Configuration Structure
typedef struct {
    char apn[64];           // Access Point Name
    char username[32];      // APN username (optional)
    char password[32];      // APN password (optional)
} sim808_gprs_config_t;

// MQTT Configuration Structure
typedef struct {
    char broker[128];       // MQTT broker address (IP or hostname)
    int port;               // MQTT broker port (usually 1883)
    char username[32];      // MQTT username
    char password[32];      // MQTT password
    char client_id[32];     // MQTT client ID
} sim808_mqtt_config_t;

// Function Prototypes

// ============================================
// Initialization Functions
// ============================================

/**
 * Initialize UART communication with SIM808
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_init(void);

/**
 * Power on SIM808 module
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_power_on(void);

/**
 * Power off SIM808 module
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_power_off(void);

// ============================================
// AT Command Functions
// ============================================

/**
 * Send AT command and get response
 * @param cmd Command string to send
 * @param response Buffer to store response
 * @param response_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t sim808_send_command(const char* cmd, char* response, size_t response_size, uint32_t timeout_ms);

/**
 * Wait for specific response string
 * @param expected Expected response string
 * @param timeout_ms Timeout in milliseconds
 * @return true if expected response received, false otherwise
 */
bool sim808_wait_for_response(const char* expected, uint32_t timeout_ms);

// ============================================
// GPS Functions
// ============================================

/**
 * Power on GPS module
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_gps_power_on(void);

/**
 * Power off GPS module
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_gps_power_off(void);

/**
 * Get GPS data
 * @param data Pointer to GPS data structure
 * @return ESP_OK on success with valid fix, ESP_FAIL on error or no fix
 */
esp_err_t sim808_gps_get_data(sim808_gps_data_t* data);

/**
 * Check if GPS has valid fix
 * @return true if GPS has fix, false otherwise
 */
bool sim808_gps_has_fix(void);

// ============================================
// GPRS Functions
// ============================================

/**
 * Connect to GPRS network
 * @param config GPRS configuration (APN, username, password)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_gprs_connect(sim808_gprs_config_t* config);

/**
 * Disconnect from GPRS network
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_gprs_disconnect(void);

/**
 * Check if GPRS is connected
 * @return true if connected, false otherwise
 */
bool sim808_gprs_is_connected(void);

// ============================================
// MQTT Functions (via AT commands)
// ============================================

/**
 * Connect to MQTT broker
 * @param config MQTT configuration (broker, port, credentials)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_mqtt_connect(sim808_mqtt_config_t* config);

/**
 * Disconnect from MQTT broker
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_mqtt_disconnect(void);

/**
 * Publish message to MQTT topic
 * @param topic MQTT topic string
 * @param payload Message payload (JSON string)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_mqtt_publish(const char* topic, const char* payload);

/**
 * Subscribe to MQTT topic
 * @param topic MQTT topic string (can include wildcards)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_mqtt_subscribe(const char* topic);

/**
 * Check if MQTT is connected
 * @return true if connected, false otherwise
 */
bool sim808_mqtt_is_connected(void);

// ============================================
// Utility Functions
// ============================================

/**
 * Get signal quality
 * @param rssi Pointer to store RSSI value (0-31, 99=unknown)
 * @param ber Pointer to store Bit Error Rate
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_get_signal_quality(int* rssi, int* ber);

/**
 * Get network registration status
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t sim808_get_network_status(void);

#endif // SIM808_H