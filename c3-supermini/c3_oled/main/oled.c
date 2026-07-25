#include <stdbool.h>
#include "oled.h"
#include "oledfont.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OLED_SDA_H()  gpio_set_level(OLED_SDA_GPIO, 1)
#define OLED_SDA_L()  gpio_set_level(OLED_SDA_GPIO, 0)
#define OLED_SCL_H()  gpio_set_level(OLED_SCL_GPIO, 1)
#define OLED_SCL_L()  gpio_set_level(OLED_SCL_GPIO, 0)

static const char *TAG = "OLED";

//==============================================================================
// Mode switching & I2C handles
//==============================================================================
static bool s_hard_i2c = false;
static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev_handle = NULL;

//==============================================================================
// Software I2C bit-bang (low-level)
//==============================================================================
static void IIC_delay(void)
{
    esp_rom_delay_us(2);
}

static void I2C_Start(void)
{
    OLED_SDA_H();
    OLED_SCL_H();
    IIC_delay();
    OLED_SDA_L();
    IIC_delay();
    OLED_SCL_L();
}

static void I2C_Stop(void)
{
    OLED_SDA_L();
    OLED_SCL_H();
    IIC_delay();
    OLED_SDA_H();
}

static void I2C_WaitAck(void)
{
    OLED_SDA_H();
    IIC_delay();
    OLED_SCL_H();
    IIC_delay();
    OLED_SCL_L();
    IIC_delay();
}

static void Send_Byte(uint8_t dat)
{
    for (uint8_t i = 0; i < 8; i++) {
        OLED_SCL_L();
        if (dat & 0x80)
            OLED_SDA_H();
        else
            OLED_SDA_L();
        IIC_delay();
        OLED_SCL_H();
        IIC_delay();
        OLED_SCL_L();
        dat <<= 1;
    }
}

static void OLED_WR_Byte_Soft(uint8_t dat, uint8_t mode)
{
    I2C_Start();
    Send_Byte(OLED_I2C_ADDR << 1);
    I2C_WaitAck();
    if (mode)
        Send_Byte(0x40);
    else
        Send_Byte(0x00);
    I2C_WaitAck();
    Send_Byte(dat);
    I2C_WaitAck();
    I2C_Stop();
}

//==============================================================================
// Hardware I2C (low-level, uses ESP-IDF driver)
//==============================================================================
static void OLED_WR_Byte_Hard(uint8_t dat, uint8_t mode)
{
    uint8_t buf[2];
    buf[0] = mode ? 0x40 : 0x00;
    buf[1] = dat;
    i2c_master_transmit(s_dev_handle, buf, 2, -1);
}

//==============================================================================
// Dispatcher – called by all higher-level functions
//==============================================================================
void OLED_WR_Byte(uint8_t dat, uint8_t mode)
{
    if (s_hard_i2c)
        OLED_WR_Byte_Hard(dat, mode);
    else
        OLED_WR_Byte_Soft(dat, mode);
}

//==============================================================================
// Cleanup
//==============================================================================
static void OLED_Deinit(void)
{
    if (s_dev_handle) {
        i2c_master_bus_rm_device(s_dev_handle);
        s_dev_handle = NULL;
    }
    if (s_bus_handle) {
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
    }
}

//==============================================================================
// OLED init sequence (common register writes)
//==============================================================================
static void OLED_Init_Sequence(void)
{
    OLED_WR_Byte(0xAE, OLED_CMD);
    OLED_WR_Byte(0xD5, OLED_CMD);
    OLED_WR_Byte(0xF0, OLED_CMD);
    OLED_WR_Byte(0xA8, OLED_CMD);
    OLED_WR_Byte(0x27, OLED_CMD);
    OLED_WR_Byte(0xD3, OLED_CMD);
    OLED_WR_Byte(0x00, OLED_CMD);
    OLED_WR_Byte(0x40, OLED_CMD);
    OLED_WR_Byte(0x8D, OLED_CMD);
    OLED_WR_Byte(0x14, OLED_CMD);
    OLED_WR_Byte(0x20, OLED_CMD);
    OLED_WR_Byte(0x02, OLED_CMD);
    OLED_WR_Byte(0xA1, OLED_CMD);
    OLED_WR_Byte(0xC8, OLED_CMD);
    OLED_WR_Byte(0xDA, OLED_CMD);
    OLED_WR_Byte(0x12, OLED_CMD);
    OLED_WR_Byte(0xAD, OLED_CMD);
    OLED_WR_Byte(0x30, OLED_CMD);
    OLED_WR_Byte(0x81, OLED_CMD);
    OLED_WR_Byte(0xFF, OLED_CMD);
    OLED_WR_Byte(0xD9, OLED_CMD);
    OLED_WR_Byte(0x22, OLED_CMD);
    OLED_WR_Byte(0xDB, OLED_CMD);
    OLED_WR_Byte(0x20, OLED_CMD);
    OLED_WR_Byte(0xA4, OLED_CMD);
    OLED_WR_Byte(0xA6, OLED_CMD);
    OLED_WR_Byte(0x0C, OLED_CMD);
    OLED_WR_Byte(0x11, OLED_CMD);
    OLED_Clear();
    OLED_WR_Byte(0xAF, OLED_CMD);
}

//==============================================================================
// Public init: software I2C
//==============================================================================
void OLED_Init(void)
{
    OLED_Deinit();

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OLED_SDA_GPIO) | (1ULL << OLED_SCL_GPIO),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    OLED_SDA_H();
    OLED_SCL_H();
    vTaskDelay(pdMS_TO_TICKS(10));

    s_hard_i2c = false;
    OLED_Init_Sequence();
    ESP_LOGI(TAG, "OLED initialized (software I2C, %dx%d)", OLED_WIDTH, OLED_HEIGHT);
}

//==============================================================================
// Public init: hardware I2C
//==============================================================================
void OLED_Hard_Init(void)
{
    OLED_Deinit();

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_dev_handle));

    s_hard_i2c = true;
    OLED_Init_Sequence();
    ESP_LOGI(TAG, "OLED initialized (hardware I2C, %dx%d)", OLED_WIDTH, OLED_HEIGHT);
}

//==============================================================================
// Display functions (common to both modes)
//==============================================================================
void OLED_ColorTurn(uint8_t i)
{
    if (i == 0)
        OLED_WR_Byte(0xA6, OLED_CMD);
    if (i == 1)
        OLED_WR_Byte(0xA7, OLED_CMD);
}

void OLED_DisplayTurn(uint8_t i)
{
    if (i == 0) {
        OLED_WR_Byte(0xC8, OLED_CMD);
        OLED_WR_Byte(0xA1, OLED_CMD);
    }
    if (i == 1) {
        OLED_WR_Byte(0xC0, OLED_CMD);
        OLED_WR_Byte(0xA0, OLED_CMD);
    }
}

void OLED_Set_Pos(uint8_t x, uint8_t y)
{
    x += 28;
    OLED_WR_Byte(0xB0 + y, OLED_CMD);
    OLED_WR_Byte(((x & 0xF0) >> 4) | 0x10, OLED_CMD);
    OLED_WR_Byte((x & 0x0F), OLED_CMD);
}

void OLED_Clear(void)
{
    for (uint8_t i = 0; i < 5; i++) {
        OLED_WR_Byte(0xB0 + i, OLED_CMD);
        OLED_WR_Byte(0x0C, OLED_CMD);
        OLED_WR_Byte(0x11, OLED_CMD);
        for (uint8_t n = 0; n < 72; n++)
            OLED_WR_Byte(0, OLED_DATA);
    }
}

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t sizey)
{
    uint8_t sizex = sizey / 2;
    uint16_t size1;
    if (sizey == 8)
        size1 = 6;
    else
        size1 = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * (sizey / 2);
    uint8_t c = chr - ' ';
    OLED_Set_Pos(x, y);
    for (uint16_t i = 0; i < size1; i++) {
        if (i % sizex == 0 && sizey != 8)
            OLED_Set_Pos(x, y++);
        if (sizey == 8)
            OLED_WR_Byte(asc2_0806[c][i], OLED_DATA);
        else if (sizey == 16)
            OLED_WR_Byte(asc2_1608[c][i], OLED_DATA);
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *chr, uint8_t sizey)
{
    while (*chr != '\0') {
        OLED_ShowChar(x, y, (uint8_t)*chr++, sizey);
        if (sizey == 8) x += 6;
        else x += sizey / 2;
    }
}


