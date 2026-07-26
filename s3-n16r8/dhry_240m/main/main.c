#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_timer.h"

#include "custom_def.h"
#include "dhry.h"
#include "ws2812_led.h"

#define WS2812_GPIO GPIO_NUM_48

static const char *TAG = "DHRY";

void app_main(void)
{
    ws2812_init(WS2812_GPIO);

    uint32_t freq = esp_clk_cpu_freq();

    ESP_LOGI(TAG, "ESP32-S3 N16R8 Dhrystone Benchmark");
    ESP_LOGI(TAG, "CPU freq: %u Hz (%u MHz)", freq, freq / 1000000);
    ESP_LOGI(TAG, "Compiler: %s", COMPILER_NAME);

    while (1) {
        ws2812_set_rgb(0, 64, 0);
        dhry_main(freq);
        ws2812_clear();

        ESP_LOGI(TAG, "CPU freq: %u Hz (%u MHz)", freq, freq / 1000000);
        ESP_LOGI(TAG, "Compiler: %s", COMPILER_NAME);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
