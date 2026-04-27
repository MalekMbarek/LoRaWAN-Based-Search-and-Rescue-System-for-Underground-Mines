/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lcd_display.h
  * @brief   LCD 20x4 display interface (I2C)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LCD_DISPLAY_H__
#define __LCD_DISPLAY_H__

#include <stdint.h>

/* LCD dimensions */
#define LCD_ROWS    2
#define LCD_COLS    20

/* Function prototypes */
void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char* str);
void LCD_PrintChar(char c);
void LCD_Backlight(uint8_t on);

#endif /* __LCD_DISPLAY_H__ */