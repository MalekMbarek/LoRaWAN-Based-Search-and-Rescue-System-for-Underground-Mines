/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lcd_display.h
  * @brief   LCD 20x4 I2C display interface for ScannerNode
  *          - I2C communication with PCF8574 I/O expander
  *          - Cursor positioning and text printing
  *          - Backlight control
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LCD_DISPLAY_H__
#define __LCD_DISPLAY_H__

#include <stdint.h>

/* ============================================================
 * LCD Dimensions
 * ============================================================ */
#define LCD_ROWS    2     /* Number of rows (2 or 4) */
#define LCD_COLS    16    /* Number of columns (16 or 20) */

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/**
  * @brief  Initialize LCD in 4-bit mode
  */
void LCD_Init(void);

/**
  * @brief  Clear LCD display
  */
void LCD_Clear(void);

/**
  * @brief  Set cursor position
  * @param  row Row position (0-3 for 4-line display)
  * @param  col Column position (0-19 for 20-column display)
  */
void LCD_SetCursor(uint8_t row, uint8_t col);

/**
  * @brief  Print a string on LCD at current cursor position
  * @param  str Null-terminated string to display
  */
void LCD_Print(const char* str);

/**
  * @brief  Print a single character on LCD
  * @param  c Character to display
  */
void LCD_PrintChar(char c);

/**
  * @brief  Control LCD backlight
  * @param  on 1 to turn on, 0 to turn off
  */
void LCD_Backlight(uint8_t on);

#endif /* __LCD_DISPLAY_H__ */