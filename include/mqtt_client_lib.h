#ifndef MQTT_CLIENT_LIB_H
#define MQTT_CLIENT_LIB_H

#include "mqtt_client.h"

// Callback type for message handling
typedef void (*mqtt_message_callback_t)(const char *topic, const char *payload);

// Initialize MQTT client
void mqtt_client_init(const char *broker_uri);

// Publish message
void mqtt_publish(const char *topic, const char *payload);

// Subscribe to topic
void mqtt_subscribe(const char *topic, mqtt_message_callback_t callback);

// Loop/task to maintain connection (if needed)
void mqtt_loop_task(void *pvParameters);

#endif // MQTT_CLIENT_LIB_H
