#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#define SIOD_GPIO   GPIO_NUM_4
#define SIOC_GPIO   GPIO_NUM_5
#define XCLK_GPIO   GPIO_NUM_15

#define XCLK_FREQ_HZ   20000000
#define I2C_FREQ_HZ    400000
#define I2C_TIMEOUT_MS 1000

#define OV5640_SCCB_ADDR 0x3C   /* 7-bit slave address (0x78 write / 0x79 read) */

/* OV5640 uses a 16-bit register address map */
#define REG_PID   0x300A   /* Product ID (MSB): 0x56 for OV5640 */
#define REG_VER   0x300B   /* Product ID (LSB): 0x40 */
#define REG_MID_H 0x300C   /* Manufacturer ID high (0x7F = OmniVision) */
#define REG_MID_L 0x300D   /* Manufacturer ID low (0xA2) */

static const char *TAG = "ov5640";

static void xclk_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = XCLK_FREQ_HZ,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t chan_cfg = {
        .gpio_num = XCLK_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_cfg));
    /* 50% duty cycle at 1-bit resolution */
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_LOGI(TAG, "XCLK on GPIO%d @ %d Hz", XCLK_GPIO, XCLK_FREQ_HZ);
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t *val)
{
    /* OV sensors use 16-bit register (sub-address) */
    uint8_t buf[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };

    /* SCCB two-phase read: write sub-address with STOP, then a fresh START + read */
    esp_err_t err = i2c_master_transmit(dev, buf, 2, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_master_receive(dev, val, 1, I2C_TIMEOUT_MS);
}

static void probe_address(i2c_master_bus_handle_t bus, uint8_t addr7)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr7,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "addr 0x%02X: add device failed", addr7);
        return;
    }

    uint8_t pid = 0, ver = 0, mid_h = 0, mid_l = 0;
    if (read_reg(dev, REG_PID, &pid) != ESP_OK) {
        ESP_LOGW(TAG, "addr 0x%02X: no response (NACK/timeout)", addr7);
        i2c_master_bus_rm_device(dev);
        return;
    }

    read_reg(dev, REG_VER, &ver);
    read_reg(dev, REG_MID_H, &mid_h);
    read_reg(dev, REG_MID_L, &mid_l);

    uint16_t device_id = ((uint16_t)pid << 8) | ver;
    ESP_LOGI(TAG, "addr 0x%02X: PID=0x%02X VER=0x%02X device_id=0x%04X MID=0x%02X%02X%s",
             addr7, pid, ver, device_id, mid_h, mid_l,
             (device_id == 0x5640) ? "  <-- OV5640" : "");

    i2c_master_bus_rm_device(dev);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32-S3 N16R8 OV5640 Device ID Test");
    ESP_LOGI(TAG, "  SIOD=GPIO%d  SIOC=GPIO%d  XCLK=GPIO%d",
             SIOD_GPIO, SIOC_GPIO, XCLK_GPIO);
    ESP_LOGI(TAG, "========================================");

    xclk_init();

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SIOD_GPIO,
        .scl_io_num = SIOC_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    ESP_LOGI(TAG, "--- Reading OV5640 device ID at addr 0x%02X ---", OV5640_SCCB_ADDR);
    probe_address(bus, OV5640_SCCB_ADDR);
    ESP_LOGI(TAG, "--- Done ---");
}
