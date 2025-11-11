#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include "driver/uart.h"

// GPS UART Configuration
#define GPS_UART_NUM        UART_NUM_2
#define GPS_TXD_PIN         GPIO_NUM_17
#define GPS_RXD_PIN         GPIO_NUM_16
#define GPS_BAUD_RATE       9600
#define GPS_UART_BUF_SIZE   1024

// GPS data structure
typedef struct {
    float latitude;
    float longitude;
    float altitude;
    char time[16];
    char date[16];
    int satellites;
    int fix_quality;
    bool valid;
} gps_data_t;

// Function prototypes
void gps_init(void);
void gps_task(void *pvParameters);
gps_data_t gps_get_data(void);
bool gps_has_fix(void);

#endif // GPS_H