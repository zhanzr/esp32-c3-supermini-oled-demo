#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO       GPIO_NUM_5

static const char *TAG = "BLINK";

void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "ESP32 Thing Blink");
    ESP_LOGI(TAG, "LED on GPIO %d", LED_GPIO);

    uint32_t tick = 0;

    while (1) {
        gpio_set_level(LED_GPIO, (tick / 16) & 1);
        tick++;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
