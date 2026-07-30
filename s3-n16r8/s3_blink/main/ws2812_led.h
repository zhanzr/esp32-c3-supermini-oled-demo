#pragma once

#include <stdint.h>

void ws2812_init(int gpio_num);
void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void ws2812_clear(void);