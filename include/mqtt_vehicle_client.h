#ifndef MQTT_VEHICLE_CLIENT_H
#define MQTT_VEHICLE_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

// MQTT Configuration
#define MQTT_BROKER_URI     "mqtt://103.175.219.138:1883"
#define MQTT_USERNAME       "vehicle"
#define MQTT_PASSWORD       "vehicle123"
#define MQTT_KEEPALIVE      60

// MQTT Topics
#define TOPIC_EXCHANGE      "vehicle.exchange"

// Message types
typedef enum {
    MSG_LOCATION,
    MSG_STATUS,
    MSG_BATTERY,
    MSG_PERFORMANCE,
    MSG_REGISTRATION
} message_type_t;

// Vehicle state
typedef struct {
    bool is_active;
    bool is_locked;
    bool is_killed;
    bool kill_scheduled;
    char order_id[64];
} vehicle_state_t;

// Function prototypes
void mqtt_vehicle_init(const char* vehicle_id);
void mqtt_vehicle_start(void);
void mqtt_vehicle_stop(void);
bool mqtt_vehicle_is_connected(void);

void mqtt_publish_location(float latitude, float longitude, float altitude);
void mqtt_publish_status(bool is_active, bool is_locked, bool is_killed);
void mqtt_publish_battery(float voltage, float battery_level);
void mqtt_publish_performance(void);
void mqtt_publish_registration(void);

vehicle_state_t* mqtt_get_vehicle_state(void);

#endif // MQTT_VEHICLE_CLIENT_H