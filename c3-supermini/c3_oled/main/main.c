#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "oled.h"

static const char *TAG = "MAIN";

static void IRAM_ATTR disable_brownout(void)
{
    REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
    ESP_LOGI(TAG, "Brownout detector disabled");
}

static void show_soft_demo(void)
{
    ESP_LOGI(TAG, "=== Software I2C Demo ===");
    OLED_Init();
    OLED_ColorTurn(0);
    OLED_DisplayTurn(0);
    OLED_ShowString(0, 0, "Hello World!", 8);
    OLED_ShowString(0, 2, "SW I2C OK", 8);
    OLED_ShowString(0, 4, "0.42 OLED", 8);
    ESP_LOGI(TAG, "Soft I2C display done");
}

static void show_hard_demo(void)
{
    ESP_LOGI(TAG, "=== Hardware I2C Demo ===");
    OLED_Hard_Init();
    OLED_ColorTurn(0);
    OLED_DisplayTurn(0);
    OLED_ShowString(0, 0, "ABCDEFGH", 8);
    OLED_ShowString(0, 2, "HW I2C OK", 8);
    OLED_ShowString(0, 4, "ESP32C3", 8);
    ESP_LOGI(TAG, "Hard I2C display done");
}

void app_main(void)
{
    disable_brownout();

    ESP_LOGI(TAG, "ESP32-C3 OLED Demo (soft vs hard I2C)");

    for (;;) {
        show_soft_demo();
        vTaskDelay(pdMS_TO_TICKS(10000));

        show_hard_demo();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
