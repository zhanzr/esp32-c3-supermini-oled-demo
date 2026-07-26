#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ws2812_led.h"

#define WS2812_GPIO GPIO_NUM_48

static const char *TAG = "S3_EMPTY";

#define MAX_BRIGHTNESS 48

static void color_cycle(uint8_t *r, uint8_t *g, uint8_t *b)
{
    static uint8_t phase = 0;
    switch (phase) {
        case 0: (*r)++; if (*r >= MAX_BRIGHTNESS) phase = 1; break;
        case 1: (*g)++; if (*g >= MAX_BRIGHTNESS) phase = 2; break;
        case 2: (*b)++; if (*b >= MAX_BRIGHTNESS) phase = 3; break;
        case 3: (*r)--; if (*r == 0) phase = 4; break;
        case 4: (*g)--; if (*g == 0) phase = 5; break;
        case 5: (*b)--; if (*b == 0) phase = 0; break;
    }
}

void app_main(void)
{
    ws2812_init(WS2812_GPIO);

    ESP_LOGI(TAG, "ESP32-S3 N16R8 WS2812 Demo");
    ESP_LOGI(TAG, "WS2812 RGB LED on GPIO %d", WS2812_GPIO);

    uint8_t r = 0, g = 0, b = 0;

    while (1) {
        color_cycle(&r, &g, &b);
        ws2812_set_rgb(r, g, b);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
