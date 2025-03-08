#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <inttypes.h>
#include <string.h>
#include "ds18b20_splitgpio.h"

static bool onewire_initialized = false;
static gpio_num_t onewire_out_gpio;
static gpio_num_t onewire_in_gpio;
static uint32_t crc_errors = 0;

static const char *TAG = "DS18B20_SPLITGPIO";

static bool onewire_reset() {
    gpio_set_level(onewire_out_gpio, 0);
    esp_rom_delay_us(480);
    gpio_set_level(onewire_out_gpio, 1);
    esp_rom_delay_us(70);
    int presence = gpio_get_level(onewire_in_gpio);
    esp_rom_delay_us(410);
    return presence == 0;
}

static void onewire_write_bit(int bit) {
    gpio_set_level(onewire_out_gpio, 0);
    if (bit) {
        esp_rom_delay_us(10);
        gpio_set_level(onewire_out_gpio, 1);
        esp_rom_delay_us(55);
    } else {
        esp_rom_delay_us(65);
        gpio_set_level(onewire_out_gpio, 1);
        esp_rom_delay_us(5);
    }
}

static int onewire_read_bit() {
    gpio_set_level(onewire_out_gpio, 0);
    esp_rom_delay_us(3);
    gpio_set_level(onewire_out_gpio, 1);
    esp_rom_delay_us(10);
    int bit = gpio_get_level(onewire_in_gpio);
    esp_rom_delay_us(53);
    return bit;
}

static void onewire_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        onewire_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t onewire_read_byte() {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (onewire_read_bit()) byte |= 0x80;
    }
    return byte;
}

static uint8_t ds18b20_crc8(const uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

esp_err_t onewire_init(gpio_num_t gpio_out, gpio_num_t gpio_in) {
    onewire_out_gpio = gpio_out;
    onewire_in_gpio = gpio_in;

    gpio_reset_pin(onewire_out_gpio);
    gpio_set_direction(onewire_out_gpio, GPIO_MODE_OUTPUT);
    gpio_reset_pin(onewire_in_gpio);
    gpio_set_direction(onewire_in_gpio, GPIO_MODE_INPUT);

    onewire_initialized = true;
    ESP_LOGI(TAG, "onewire bus initialized successfully");
    return ESP_OK;
}

esp_err_t ds18b20_search_sensors(onewire_addr_t *devices, int *num_devices) {
    *num_devices = 0;
    if (!onewire_initialized) return ESP_ERR_INVALID_STATE;

    uint8_t rom[8];
    int device_count = 0;
    uint8_t last_discrepancy = 0;
    uint8_t last_device_flag = 0;

    while (!last_device_flag && device_count < MAX_SENSORS) {
        if (!onewire_reset()) break;

        onewire_write_byte(0xF0); // SEARCH ROM command

        uint8_t rom_byte_number = 0;
        uint8_t rom_byte_mask = 1;
        memset(rom, 0, sizeof(rom));

        for (int id_bit_number = 1; id_bit_number <= 64; id_bit_number++) {
            int id_bit = onewire_read_bit();
            int cmp_id_bit = onewire_read_bit();

            int search_direction;

            if (id_bit == 1 && cmp_id_bit == 1) break;
            if (id_bit == 0 && cmp_id_bit == 0) {
                if (id_bit_number == last_discrepancy) {
                    search_direction = 1;
                } else if (id_bit_number > last_discrepancy) {
                    search_direction = 0;
                    last_discrepancy = id_bit_number;
                } else {
                    search_direction = ((rom[rom_byte_number] & rom_byte_mask) ? 1 : 0);
                }
            } else {
                search_direction = id_bit;
            }

            if (search_direction == 1) rom[rom_byte_number] |= rom_byte_mask;

            onewire_write_bit(search_direction);

            rom_byte_mask <<= 1;
            if (rom_byte_mask == 0) {
                rom_byte_number++;
                rom_byte_mask = 1;
            }
        }

        if (ds18b20_crc8(rom, 7) != rom[7]) continue;

        memcpy(&devices[device_count++], rom, 8);

        if (last_discrepancy == 0) {
            last_device_flag = 1;
        }

        if (last_device_flag || device_count >= MAX_SENSORS) break;
    }

    *num_devices = device_count;
    return (device_count > 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t ds18b20_get_temperature(onewire_addr_t addr, float *temperature) {
    if (!onewire_initialized) {
        ESP_LOGE(TAG, "onewire not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!onewire_reset()) return ESP_ERR_TIMEOUT;

    onewire_write_byte(0x55); // MATCH ROM
    for (int i = 0; i < 8; i++) onewire_write_byte((addr >> (8 * i)) & 0xFF);
    onewire_write_byte(0x44); // CONVERT T
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!onewire_reset()) return ESP_ERR_TIMEOUT;

    onewire_write_byte(0x55); // MATCH ROM
    for (int i = 0; i < 8; i++) onewire_write_byte((addr >> (8 * i)) & 0xFF);
    onewire_write_byte(0xBE); // READ SCRATCHPAD

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) scratchpad[i] = onewire_read_byte();

    uint8_t calculated_crc = ds18b20_crc8(scratchpad, 8);
    if (calculated_crc != scratchpad[8]) {
        ESP_LOGE(TAG, "CRC mismatch: expected 0x%02X, got 0x%02X", calculated_crc, scratchpad[8]);
        crc_errors++;
        return ESP_ERR_INVALID_CRC;
    }

    int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
    *temperature = raw_temp / 16.0;
    return ESP_OK;
}

uint32_t ds18b20_get_crc_errors() {
    return crc_errors;
}
