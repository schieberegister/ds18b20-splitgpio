#ifndef DS18B20_SPLITGPIO_H
#define DS18B20_SPLITGPIO_H

#include <esp_err.h>
#include <driver/gpio.h>
#include <inttypes.h>

typedef uint64_t onewire_addr_t;

#define MAX_SENSORS 8

esp_err_t onewire_init(gpio_num_t gpio_out, gpio_num_t gpio_in);
esp_err_t ds18b20_search_sensors(onewire_addr_t *devices, int *num_devices);
esp_err_t ds18b20_get_temperature(onewire_addr_t addr, float *temperature);
uint32_t ds18b20_get_crc_errors();

#endif // DS18B20_SPLITGPIO_H

