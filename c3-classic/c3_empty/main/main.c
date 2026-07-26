#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO GPIO_NUM_12

static const char *TAG = "BLINK_APP";

void app_main(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Blink application started on GPIO %d", BLINK_GPIO);

    uint32_t level = 0;

    while (1) {
        gpio_set_level(BLINK_GPIO, level);
        ESP_LOGI(TAG, "LED State: %s", level ? "ON" : "OFF");

        level = !level;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
