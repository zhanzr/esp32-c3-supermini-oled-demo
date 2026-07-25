#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#define OLED_SDA_GPIO   5
#define OLED_SCL_GPIO   6
#define OLED_I2C_ADDR   0x3C

#define OLED_WIDTH  72
#define OLED_HEIGHT 40

#define OLED_CMD   0
#define OLED_DATA  1

void OLED_Init(void);
void OLED_Hard_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t sizey);
void OLED_ShowString(uint8_t x, uint8_t y, const char *chr, uint8_t sizey);
void OLED_ColorTurn(uint8_t i);
void OLED_DisplayTurn(uint8_t i);

#endif
