#ifndef GPS_H
#define GPS_H

#include <stdbool.h>

// UART Configuration for GPS
#define GPS_UART_NUM        UART_NUM_1
#define GPS_TXD_PIN         GPIO_NUM_17
#define GPS_RXD_PIN         GPIO_NUM_16
#define GPS_UART_BUF_SIZE   1024
#define GPS_BAUD_RATE       9600

typedef struct {
    float latitude;
    float longitude;
    float altitude;
    int satellites;
    int fix_quality;
    char time[11];
    char date[11];
    bool valid;
} gps_data_t;

void gps_init(void);
void gps_task(void *pvParameters);
gps_data_t gps_get_data(void);
bool gps_has_fix(void);

#endif
