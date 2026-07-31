#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"

static const char *TAG = "S3_EMPTY";

void app_main(void)
{
    uint32_t freq = esp_clk_cpu_freq();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32-S3 N8R8 OV3660 Camera Board");
    ESP_LOGI(TAG, "  No GPIO LED and no WS2812 LED present");
    ESP_LOGI(TAG, "  CPU freq: %u MHz", freq / 1000000);
    ESP_LOGI(TAG, "========================================");

    while (1) {
        ESP_LOGI(TAG, "Board alive, no LEDs present on this board (OV3660 camera not yet driven)");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
