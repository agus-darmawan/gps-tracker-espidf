#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include "esp_err.h"
#include <stdbool.h>

// Configuration structure
typedef struct {
    char vehicle_id[32];
    bool is_configured;
} vehicle_config_t;

// Function prototypes
void web_config_init(void);
void web_config_load(void);
void web_config_save(const char* vehicle_id);
bool web_config_is_configured(void);
const char* web_config_get_vehicle_id(void);
vehicle_config_t* web_config_get(void);

#endif // WEB_CONFIG_H