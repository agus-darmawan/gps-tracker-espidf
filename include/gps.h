#ifndef GPS_H
#define GPS_H

#include <stdbool.h>

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