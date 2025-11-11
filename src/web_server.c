#include "web_server.h"
#include "web_html.h"
#include "web_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

/**
 * HTTP GET handler for root page
 * Serves the configuration HTML page
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, config_html_page, strlen(config_html_page));
    ESP_LOGI(TAG, "Served configuration page");
    return ESP_OK;
}

/**
 * HTTP POST handler for saving configuration
 * Processes vehicle ID submission
 */
static esp_err_t save_post_handler(httpd_req_t *req) {
    char content[128];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        ESP_LOGE(TAG, "Failed to receive data");
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    ESP_LOGI(TAG, "Received POST data: %s", content);
    
    // Parse vehicle_id from POST data
    char vehicle_id[32] = {0};
    char *start = strstr(content, "vehicle_id=");
    
    if (start) {
        start += strlen("vehicle_id=");
        char *end = strchr(start, '&');
        int len = end ? (end - start) : strlen(start);
        
        if (len > 0 && len < sizeof(vehicle_id)) {
            strncpy(vehicle_id, start, len);
            vehicle_id[len] = '\0';
            
            // URL decode: replace '+' with space
            for (int i = 0; vehicle_id[i]; i++) {
                if (vehicle_id[i] == '+') {
                    vehicle_id[i] = ' ';
                }
            }
            
            // URL decode: handle %XX encoding
            char decoded[32] = {0};
            int j = 0;
            for (int i = 0; vehicle_id[i] && j < sizeof(decoded) - 1; i++) {
                if (vehicle_id[i] == '%' && vehicle_id[i+1] && vehicle_id[i+2]) {
                    char hex[3] = {vehicle_id[i+1], vehicle_id[i+2], 0};
                    decoded[j++] = (char)strtol(hex, NULL, 16);
                    i += 2;
                } else {
                    decoded[j++] = vehicle_id[i];
                }
            }
            decoded[j] = '\0';
            
            // Save configuration
            web_config_save(decoded);
            
            // Send success response
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_send(req, "OK", 2);
            
            ESP_LOGI(TAG, "Configuration saved: %s", decoded);
            return ESP_OK;
        }
    }
    
    // Invalid data
    ESP_LOGW(TAG, "Invalid vehicle ID in POST data");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Invalid vehicle ID", 17);
    return ESP_FAIL;
}

/**
 * HTTP GET handler for status check
 * Returns current configuration status
 */
static esp_err_t status_get_handler(httpd_req_t *req) {
    char response[128];
    
    if (web_config_is_configured()) {
        snprintf(response, sizeof(response), 
                "{\"configured\":true,\"vehicle_id\":\"%s\"}", 
                web_config_get_vehicle_id());
    } else {
        snprintf(response, sizeof(response), 
                "{\"configured\":false}");
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/**
 * Initialize web server
 */
void web_server_init(void) {
    ESP_LOGI(TAG, "Web server module initialized");
}

/**
 * Start HTTP server
 */
void web_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;
    config.backlog_conn = 5;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);
        
        httpd_uri_t uri_post = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);
        
        httpd_uri_t uri_status = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_status);
        
        ESP_LOGI(TAG, "HTTP server started successfully");
        ESP_LOGI(TAG, "Registered handlers: /, /save, /status");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        server = NULL;
    }
}

/**
 * Stop HTTP server
 */
void web_server_stop(void) {
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

/**
 * Check if web server is running
 */
bool web_server_is_running(void) {
    return (server != NULL);
}