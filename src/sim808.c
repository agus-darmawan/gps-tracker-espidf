#include "sim808.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SIM808";
static bool gps_powered = false;
static bool gprs_connected = false;
static bool mqtt_connected = false;

/**
 * Initialize UART for SIM808 communication
 */
esp_err_t sim808_init(void) {
    uart_config_t uart_config = {
        .baud_rate = SIM808_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    esp_err_t ret = uart_param_config(SIM808_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART");
        return ret;
    }
    
    ret = uart_set_pin(SIM808_UART_NUM, SIM808_TX_PIN, SIM808_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins");
        return ret;
    }
    
    ret = uart_driver_install(SIM808_UART_NUM, SIM808_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        return ret;
    }
    
    // Configure control pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SIM808_POWER_PIN) | (1ULL << SIM808_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    gpio_set_level(SIM808_POWER_PIN, 0);
    gpio_set_level(SIM808_RST_PIN, 1);
    
    ESP_LOGI(TAG, "SIM808 UART initialized");
    return ESP_OK;
}

/**
 * Power on SIM808 module
 */
esp_err_t sim808_power_on(void) {
    ESP_LOGI(TAG, "Powering on SIM808...");
    
    // Power pulse (>1 second)
    gpio_set_level(SIM808_POWER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(SIM808_POWER_PIN, 0);
    
    // Wait for module to boot
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Test communication
    char response[128];
    if (sim808_send_command("AT\r\n", response, sizeof(response), 2000) != ESP_OK) {
        ESP_LOGE(TAG, "No response from SIM808");
        return ESP_FAIL;
    }
    
    // Disable echo
    sim808_send_command("ATE0\r\n", response, sizeof(response), 1000);
    
    // Set SMS text mode
    sim808_send_command("AT+CMGF=1\r\n", response, sizeof(response), 1000);
    
    ESP_LOGI(TAG, "SIM808 powered on successfully");
    return ESP_OK;
}

/**
 * Power off SIM808 module
 */
esp_err_t sim808_power_off(void) {
    char response[128];
    sim808_send_command("AT+CPOWD=1\r\n", response, sizeof(response), 5000);
    
    ESP_LOGI(TAG, "SIM808 powered off");
    return ESP_OK;
}

/**
 * Send AT command and wait for response
 */
esp_err_t sim808_send_command(const char* cmd, char* response, size_t response_size, uint32_t timeout_ms) {
    // Clear UART buffer
    uart_flush(SIM808_UART_NUM);
    
    // Send command
    uart_write_bytes(SIM808_UART_NUM, cmd, strlen(cmd));
    ESP_LOGD(TAG, "Sent: %s", cmd);
    
    // Wait for response
    int len = uart_read_bytes(SIM808_UART_NUM, (uint8_t*)response, response_size - 1, 
                              timeout_ms / portTICK_PERIOD_MS);
    
    if (len > 0) {
        response[len] = '\0';
        ESP_LOGD(TAG, "Received: %s", response);
        return ESP_OK;
    }
    
    response[0] = '\0';
    return ESP_ERR_TIMEOUT;
}

/**
 * Wait for specific response
 */
bool sim808_wait_for_response(const char* expected, uint32_t timeout_ms) {
    char buffer[256];
    TickType_t start_time = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
        int len = uart_read_bytes(SIM808_UART_NUM, (uint8_t*)buffer, sizeof(buffer) - 1, 
                                  100 / portTICK_PERIOD_MS);
        if (len > 0) {
            buffer[len] = '\0';
            if (strstr(buffer, expected) != NULL) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Turn on GPS
 */
esp_err_t sim808_gps_power_on(void) {
    char response[128];
    
    ESP_LOGI(TAG, "Turning on GPS...");
    
    // Power on GPS
    if (sim808_send_command("AT+CGNSPWR=1\r\n", response, sizeof(response), 2000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on GPS");
        return ESP_FAIL;
    }
    
    if (strstr(response, "OK") == NULL) {
        ESP_LOGE(TAG, "GPS power on failed");
        return ESP_FAIL;
    }
    
    gps_powered = true;
    ESP_LOGI(TAG, "GPS powered on, waiting for fix...");
    
    return ESP_OK;
}

/**
 * Turn off GPS
 */
esp_err_t sim808_gps_power_off(void) {
    char response[128];
    sim808_send_command("AT+CGNSPWR=0\r\n", response, sizeof(response), 2000);
    gps_powered = false;
    ESP_LOGI(TAG, "GPS powered off");
    return ESP_OK;
}

/**
 * Parse GNSS data from AT+CGNSINF response
 * Format: +CGNSINF: <GNSS run status>,<Fix status>,<UTC date & Time>,<Latitude>,<Longitude>,
 *         <MSL Altitude>,<Speed Over Ground>,<Course Over Ground>,<Fix Mode>,<Reserved1>,
 *         <HDOP>,<PDOP>,<VDOP>,<Reserved2>,<GNSS Satellites in View>,<HPA>,<VPA>
 */
static bool parse_gnss_data(const char* response, sim808_gps_data_t* data) {
    char* line = strstr(response, "+CGNSINF:");
    if (line == NULL) {
        return false;
    }
    
    line += strlen("+CGNSINF:");
    
    int run_status, fix_status;
    char datetime[24];
    float lat, lon, alt, speed, course;
    int fix_mode, sat_view;
    float hdop, pdop, vdop;
    
    int parsed = sscanf(line, "%d,%d,%[^,],%f,%f,%f,%f,%f,%d,%*f,%f,%f,%f,%*d,%d",
                       &run_status, &fix_status, datetime, &lat, &lon, &alt, 
                       &speed, &course, &fix_mode, &hdop, &pdop, &vdop, &sat_view);
    
    if (parsed < 13) {
        return false;
    }
    
    data->valid = (fix_status == 1);
    data->latitude = lat;
    data->longitude = lon;
    data->altitude = alt;
    data->speed = speed;
    data->satellites = sat_view;
    
    // Parse datetime (format: yyyyMMddHHmmss.sss)
    if (strlen(datetime) >= 14) {
        snprintf(data->timestamp, sizeof(data->timestamp), 
                "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
                datetime, datetime+4, datetime+6, 
                datetime+8, datetime+10, datetime+12);
        snprintf(data->date, sizeof(data->date), 
                "%.4s-%.2s-%.2s", datetime, datetime+4, datetime+6);
    }
    
    return data->valid;
}

/**
 * Get GPS data
 */
esp_err_t sim808_gps_get_data(sim808_gps_data_t* data) {
    if (!gps_powered) {
        ESP_LOGW(TAG, "GPS is not powered on");
        return ESP_ERR_INVALID_STATE;
    }
    
    char response[512];
    if (sim808_send_command("AT+CGNSINF\r\n", response, sizeof(response), 2000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get GPS data");
        return ESP_FAIL;
    }
    
    if (parse_gnss_data(response, data)) {
        ESP_LOGD(TAG, "GPS: %.6f, %.6f, alt=%.2f, speed=%.2f, sats=%d", 
                data->latitude, data->longitude, data->altitude, data->speed, data->satellites);
        return ESP_OK;
    }
    
    data->valid = false;
    return ESP_FAIL;
}

/**
 * Check if GPS has fix
 */
bool sim808_gps_has_fix(void) {
    sim808_gps_data_t data;
    return (sim808_gps_get_data(&data) == ESP_OK && data.valid);
}

/**
 * Connect to GPRS network
 */
esp_err_t sim808_gprs_connect(sim808_gprs_config_t* config) {
    char response[256];
    char cmd[128];
    
    ESP_LOGI(TAG, "Connecting to GPRS network...");
    
    // Check network registration
    sim808_send_command("AT+CREG?\r\n", response, sizeof(response), 2000);
    
    // Set bearer profile
    sim808_send_command("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n", response, sizeof(response), 2000);
    
    snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"\r\n", config->apn);
    sim808_send_command(cmd, response, sizeof(response), 2000);
    
    if (strlen(config->username) > 0) {
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"USER\",\"%s\"\r\n", config->username);
        sim808_send_command(cmd, response, sizeof(response), 2000);
    }
    
    if (strlen(config->password) > 0) {
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"PWD\",\"%s\"\r\n", config->password);
        sim808_send_command(cmd, response, sizeof(response), 2000);
    }
    
    // Open bearer
    sim808_send_command("AT+SAPBR=1,1\r\n", response, sizeof(response), 10000);
    
    // Check bearer status
    if (sim808_send_command("AT+SAPBR=2,1\r\n", response, sizeof(response), 2000) == ESP_OK) {
        if (strstr(response, "1,1") != NULL) {
            gprs_connected = true;
            ESP_LOGI(TAG, "GPRS connected successfully");
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Failed to connect to GPRS");
    return ESP_FAIL;
}

/**
 * Disconnect from GPRS network
 */
esp_err_t sim808_gprs_disconnect(void) {
    char response[128];
    sim808_send_command("AT+SAPBR=0,1\r\n", response, sizeof(response), 5000);
    gprs_connected = false;
    ESP_LOGI(TAG, "GPRS disconnected");
    return ESP_OK;
}

/**
 * Check if GPRS is connected
 */
bool sim808_gprs_is_connected(void) {
    return gprs_connected;
}

/**
 * Connect to MQTT broker (RabbitMQ)
 */
esp_err_t sim808_mqtt_connect(sim808_mqtt_config_t* config) {
    char response[256];
    char cmd[256];
    
    if (!gprs_connected) {
        ESP_LOGE(TAG, "GPRS is not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting to MQTT broker (RabbitMQ)...");
    
    // Initialize TCP/IP application
    sim808_send_command("AT+CIPSHUT\r\n", response, sizeof(response), 2000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Set single connection mode
    sim808_send_command("AT+CIPMUX=0\r\n", response, sizeof(response), 2000);
    
    // Start TCP connection to MQTT broker
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", 
             config->broker, config->port);
    
    if (sim808_send_command(cmd, response, sizeof(response), 10000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to broker");
        return ESP_FAIL;
    }
    
    if (!sim808_wait_for_response("CONNECT OK", 10000)) {
        ESP_LOGE(TAG, "TCP connection failed");
        return ESP_FAIL;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Build MQTT CONNECT packet
    uint8_t connect_packet[256];
    int packet_len = 0;
    
    // Fixed header
    connect_packet[packet_len++] = 0x10;  // CONNECT message type
    int remaining_length_pos = packet_len;
    connect_packet[packet_len++] = 0;     // Placeholder for remaining length
    
    // Variable header - Protocol Name
    connect_packet[packet_len++] = 0x00;
    connect_packet[packet_len++] = 0x04;
    connect_packet[packet_len++] = 'M';
    connect_packet[packet_len++] = 'Q';
    connect_packet[packet_len++] = 'T';
    connect_packet[packet_len++] = 'T';
    
    // Protocol Level (4 for MQTT 3.1.1)
    connect_packet[packet_len++] = 0x04;
    
    // Connect Flags (username and password)
    connect_packet[packet_len++] = 0xC2;  // Clean session + username + password
    
    // Keep Alive (60 seconds)
    connect_packet[packet_len++] = 0x00;
    connect_packet[packet_len++] = 0x3C;
    
    // Payload - Client ID
    int client_id_len = strlen(config->client_id);
    connect_packet[packet_len++] = (client_id_len >> 8) & 0xFF;
    connect_packet[packet_len++] = client_id_len & 0xFF;
    memcpy(&connect_packet[packet_len], config->client_id, client_id_len);
    packet_len += client_id_len;
    
    // Payload - Username
    int username_len = strlen(config->username);
    connect_packet[packet_len++] = (username_len >> 8) & 0xFF;
    connect_packet[packet_len++] = username_len & 0xFF;
    memcpy(&connect_packet[packet_len], config->username, username_len);
    packet_len += username_len;
    
    // Payload - Password
    int password_len = strlen(config->password);
    connect_packet[packet_len++] = (password_len >> 8) & 0xFF;
    connect_packet[packet_len++] = password_len & 0xFF;
    memcpy(&connect_packet[packet_len], config->password, password_len);
    packet_len += password_len;
    
    // Update remaining length
    connect_packet[remaining_length_pos] = packet_len - 2;
    
    // Send MQTT CONNECT packet
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", packet_len);
    sim808_send_command(cmd, response, sizeof(response), 2000);
    
    if (!sim808_wait_for_response(">", 5000)) {
        ESP_LOGE(TAG, "Failed to enter send mode");
        return ESP_FAIL;
    }
    
    uart_write_bytes(SIM808_UART_NUM, (const char*)connect_packet, packet_len);
    
    if (sim808_wait_for_response("SEND OK", 5000)) {
        // Wait for CONNACK
        vTaskDelay(pdMS_TO_TICKS(2000));
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected to RabbitMQ");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "MQTT connection failed");
    return ESP_FAIL;
}

/**
 * Disconnect from MQTT broker
 */
esp_err_t sim808_mqtt_disconnect(void) {
    char response[128];
    
    // Send MQTT DISCONNECT packet
    uint8_t disconnect_packet[] = {0xE0, 0x00};
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", sizeof(disconnect_packet));
    sim808_send_command(cmd, response, sizeof(response), 2000);
    
    if (sim808_wait_for_response(">", 5000)) {
        uart_write_bytes(SIM808_UART_NUM, (const char*)disconnect_packet, sizeof(disconnect_packet));
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Close TCP connection
    sim808_send_command("AT+CIPCLOSE\r\n", response, sizeof(response), 5000);
    
    mqtt_connected = false;
    ESP_LOGI(TAG, "MQTT disconnected");
    return ESP_OK;
}

/**
 * Publish message to MQTT topic
 */
esp_err_t sim808_mqtt_publish(const char* topic, const char* payload) {
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT is not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    char response[128];
    char cmd[64];
    
    int topic_len = strlen(topic);
    int payload_len = strlen(payload);
    
    // Build MQTT PUBLISH packet
    uint8_t publish_packet[1024];
    int packet_len = 0;
    
    // Fixed header
    publish_packet[packet_len++] = 0x30;  // PUBLISH, QoS 0
    
    // Remaining length (topic length + payload length + 2 bytes for topic length field)
    int remaining_length = 2 + topic_len + payload_len;
    publish_packet[packet_len++] = remaining_length & 0xFF;
    if (remaining_length > 127) {
        publish_packet[packet_len - 1] |= 0x80;
        publish_packet[packet_len++] = (remaining_length >> 7) & 0xFF;
    }
    
    // Topic name
    publish_packet[packet_len++] = (topic_len >> 8) & 0xFF;
    publish_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&publish_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // Payload
    memcpy(&publish_packet[packet_len], payload, payload_len);
    packet_len += payload_len;
    
    // Send packet
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", packet_len);
    sim808_send_command(cmd, response, sizeof(response), 2000);
    
    if (!sim808_wait_for_response(">", 5000)) {
        ESP_LOGE(TAG, "Failed to enter send mode");
        return ESP_FAIL;
    }
    
    uart_write_bytes(SIM808_UART_NUM, (const char*)publish_packet, packet_len);
    
    if (sim808_wait_for_response("SEND OK", 5000)) {
        ESP_LOGD(TAG, "Published to %s: %s", topic, payload);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Publish failed");
    return ESP_FAIL;
}

/**
 * Subscribe to MQTT topic
 */
esp_err_t sim808_mqtt_subscribe(const char* topic) {
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT is not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    char response[128];
    char cmd[64];
    
    int topic_len = strlen(topic);
    
    // Build MQTT SUBSCRIBE packet
    uint8_t subscribe_packet[256];
    int packet_len = 0;
    
    // Fixed header
    subscribe_packet[packet_len++] = 0x82;  // SUBSCRIBE
    subscribe_packet[packet_len++] = 2 + 2 + topic_len + 1;  // Remaining length
    
    // Packet identifier
    subscribe_packet[packet_len++] = 0x00;
    subscribe_packet[packet_len++] = 0x01;
    
    // Topic filter
    subscribe_packet[packet_len++] = (topic_len >> 8) & 0xFF;
    subscribe_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&subscribe_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // QoS
    subscribe_packet[packet_len++] = 0x00;
    
    // Send packet
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", packet_len);
    sim808_send_command(cmd, response, sizeof(response), 2000);
    
    if (!sim808_wait_for_response(">", 5000)) {
        ESP_LOGE(TAG, "Failed to enter send mode");
        return ESP_FAIL;
    }
    
    uart_write_bytes(SIM808_UART_NUM, (const char*)subscribe_packet, packet_len);
    
    if (sim808_wait_for_response("SEND OK", 5000)) {
        ESP_LOGI(TAG, "Subscribed to topic: %s", topic);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Subscribe failed");
    return ESP_FAIL;
}

/**
 * Check if MQTT is connected
 */
bool sim808_mqtt_is_connected(void) {
    return mqtt_connected;
}

/**
 * Get signal quality
 */
esp_err_t sim808_get_signal_quality(int* rssi, int* ber) {
    char response[128];
    
    if (sim808_send_command("AT+CSQ\r\n", response, sizeof(response), 2000) != ESP_OK) {
        return ESP_FAIL;
    }
    
    char* csq = strstr(response, "+CSQ:");
    if (csq != NULL) {
        sscanf(csq + 5, "%d,%d", rssi, ber);
        ESP_LOGD(TAG, "Signal: RSSI=%d, BER=%d", *rssi, *ber);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

/**
 * Get network registration status
 */
esp_err_t sim808_get_network_status(void) {
    char response[128];
    
    if (sim808_send_command("AT+CREG?\r\n", response, sizeof(response), 2000) != ESP_OK) {
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Network status: %s", response);
    return ESP_OK;
}