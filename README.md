# DS18B20 Split GPIO ESP-IDF Component

This ESP-IDF component provides functionality for interfacing DS18B20 temperature sensors using separate GPIO pins for input and output (split GPIO configuration). It is specifically designed to support environments that require distinct GPIO handling.

## Features

- Fully compatible with the Shelly Plus Add-on.
- Component for esp-idf.

## Installation

Place the ds18b20_splitgpio folder into your project's components directory:

<your_project>/components/ds18b20_splitgpio

## Usage

Include the header in your project code:

#include "ds18b20_splitgpio.h"

## API Documentation

### Functions

esp_err_t onewire_init(gpio_num_t gpio_out, gpio_num_t gpio_in)Initializes the OneWire bus with specified output and input GPIO pins.

esp_err_t ds18b20_search_sensors(onewire_addr_t *devices, int *num_devices)Searches for DS18B20 sensors connected to the bus and returns their addresses.

esp_err_t ds18b20_get_temperature(onewire_addr_t addr, float *temperature)Retrieves the temperature from a sensor with the specified address.

uint32_t ds18b20_get_crc_errors()Returns the number of CRC errors encountered during sensor data communication.

Constants

MAX_SENSORS: Maximum number of DS18B20 sensors supported (default is 8).

