#include "ws2812_led.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ws2812";

static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;

static const rmt_symbol_word_t s_bit0 = {
    .duration0 = 3,
    .level0 = 1,
    .duration1 = 8,
    .level1 = 0,
};

static const rmt_symbol_word_t s_bit1 = {
    .duration0 = 7,
    .level0 = 1,
    .duration1 = 6,
    .level1 = 0,
};

void ws2812_init(int gpio_num)
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 10 * 1000 * 1000,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &s_led_chan));

    rmt_copy_encoder_config_t encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &s_led_encoder));

    ESP_ERROR_CHECK(rmt_enable(s_led_chan));

    ESP_LOGI(TAG, "WS2812 initialized on GPIO %d", gpio_num);
}

void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    rmt_symbol_word_t symbols[24];
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    for (int i = 23; i >= 0; i--) {
        symbols[23 - i] = (grb & ((uint32_t)1 << i)) ? s_bit1 : s_bit0;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    ESP_ERROR_CHECK(rmt_transmit(s_led_chan, s_led_encoder, symbols, sizeof(symbols), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_led_chan, portMAX_DELAY));
}

void ws2812_clear(void)
{
    ws2812_set_rgb(0, 0, 0);
}
