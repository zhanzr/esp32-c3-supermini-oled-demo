#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "custom_def.h"
#include "dhry.h"

#define BLINK_GPIO GPIO_NUM_8

static const char *TAG = "DHRY";

void app_main(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint32_t freq = esp_clk_cpu_freq();

    ESP_LOGI(TAG, "ESP32-C3 Dhrystone Benchmark");
    ESP_LOGI(TAG, "CPU freq: %u Hz (%u MHz)", freq, freq / 1000000);
    ESP_LOGI(TAG, "Compiler: %s", COMPILER_NAME);

    while (1) {
        gpio_set_level(BLINK_GPIO, 1);
        dhry_main(freq);
        gpio_set_level(BLINK_GPIO, 0);

        ESP_LOGI(TAG, "CPU freq: %u Hz (%u MHz)", freq, freq / 1000000);
        ESP_LOGI(TAG, "Compiler: %s", COMPILER_NAME);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
