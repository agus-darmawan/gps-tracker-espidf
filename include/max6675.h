#ifndef MAX6675_H
#define MAX6675_H

#include "driver/spi_master.h"

// SPI Configuration for MAX6675
#define MAX6675_CS_PIN    5  // CS PIN
#define MAX6675_CLK_PIN   23 // SCK PIN
#define MAX6675_MISO_PIN  19 // SO PIN
#define SPI_SPEED_HZ      1000000  

esp_err_t max6675_init(void);
float max6675_read_temperature(void);

#endif