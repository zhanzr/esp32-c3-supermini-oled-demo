#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO GPIO_NUM_8

static const char *TAG = "BLINK_APP";

void app_main(void)
{
    // Configure GPIO 8 as an output
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "Blink application started on GPIO %d", BLINK_GPIO);

    uint32_t level = 0;

    while (1) {
      // Toggle LED state
      gpio_set_level(BLINK_GPIO, level);
      ESP_LOGI(TAG, "LED State: %s", level ? "ON" : "OFF");

      level = !level;

      // Delay for 1000 ms (1 second) using FreeRTOS tick delay
      vTaskDelay(pdMS_TO_TICKS(500));
    }
}
