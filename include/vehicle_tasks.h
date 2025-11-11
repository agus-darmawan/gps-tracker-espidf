#ifndef VEHICLE_TASKS_H
#define VEHICLE_TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Task priorities
#define GPS_TASK_PRIORITY           5
#define TRACKING_TASK_PRIORITY      5
#define MONITOR_TASK_PRIORITY       3

// Task stack sizes
#define GPS_TASK_STACK_SIZE         4096
#define TRACKING_TASK_STACK_SIZE    8192
#define MONITOR_TASK_STACK_SIZE     3072

// Task handles (extern for access from main)
extern TaskHandle_t gps_task_handle;
extern TaskHandle_t tracking_task_handle;
extern TaskHandle_t monitor_task_handle;

// Task functions
void gps_reading_task(void *pvParameters);
void vehicle_tracking_task(void *pvParameters);
void system_monitor_task(void *pvParameters);

// Task management functions
void vehicle_tasks_init(void);
void vehicle_tasks_start(void);
void vehicle_tasks_stop(void);

#endif // VEHICLE_TASKS_H