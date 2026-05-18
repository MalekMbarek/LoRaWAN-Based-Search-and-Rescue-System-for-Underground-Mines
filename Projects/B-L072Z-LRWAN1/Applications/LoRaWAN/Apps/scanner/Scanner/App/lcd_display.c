/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lcd_display.c
  * @brief   LCD 20x4 I2C implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "lcd_display.h"
#include "stm32l0xx_hal.h"
#include <string.h>

/* I2C LCD address (typically 0x27 or 0x3F) */
#define LCD_I2C_ADDR    0x27 << 1  // Shifted for HAL

/* LCD commands */
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x06
#define LCD_CMD_DISPLAY_ON      0x0C
#define LCD_CMD_DISPLAY_OFF     0x08
#define LCD_CMD_FUNCTION_SET    0x28  // 4-bit, 2 lines, 5x8
#define LCD_CMD_SET_DDRAM       0x80

/* Backlight control */
#define LCD_BACKLIGHT_ON        0x08
#define LCD_BACKLIGHT_OFF       0x00

/* Timing */
#define LCD_DELAY_MS            5

/* External I2C handle (from main.c or i2c.c) */
extern I2C_HandleTypeDef hi2c1;

static uint8_t backlightState = LCD_BACKLIGHT_ON;

/* Low-level I2C functions */
static void LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = nibble | rs | backlightState;
    
    // Send with EN high
    uint8_t buf[4];
    buf[0] = data | 0x04;  // EN = 1
    buf[1] = data | 0x04;  // EN = 1
    buf[2] = data & ~0x04; // EN = 0
    buf[3] = data & ~0x04; // EN = 0
    
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, buf, 4, 100);
    HAL_Delay(1);
}

static void LCD_SendByte(uint8_t byte, uint8_t rs)
{
    LCD_SendNibble(byte & 0xF0, rs);        // High nibble
    LCD_SendNibble((byte << 4) & 0xF0, rs); // Low nibble
}

static void LCD_SendCommand(uint8_t cmd)
{
    LCD_SendByte(cmd, 0);  // RS = 0 for command
    HAL_Delay(LCD_DELAY_MS);
}

static void LCD_SendData(uint8_t data)
{
    LCD_SendByte(data, 0x01);  // RS = 1 for data
}

/* Public functions */
void LCD_Init(void)
{
    HAL_Delay(50);  // Wait for LCD power-up
    
    // Initialize in 4-bit mode
    LCD_SendNibble(0x30, 0);
    HAL_Delay(5);
    LCD_SendNibble(0x30, 0);
    HAL_Delay(1);
    LCD_SendNibble(0x30, 0);
    HAL_Delay(1);
    LCD_SendNibble(0x20, 0);  // Switch to 4-bit mode
    HAL_Delay(1);
    
    // Function set: 4-bit, 2 lines, 5x8 font
    LCD_SendCommand(LCD_CMD_FUNCTION_SET);
    
    // Display on, cursor off, blink off
    LCD_SendCommand(LCD_CMD_DISPLAY_ON);
    
    // Clear display
    LCD_Clear();
    
    // Entry mode: increment, no shift
    LCD_SendCommand(LCD_CMD_ENTRY_MODE);
    
    HAL_Delay(10);
}

void LCD_Clear(void)
{
    LCD_SendCommand(LCD_CMD_CLEAR);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40};  // 16*2 LCD
    
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    
    LCD_SendCommand(LCD_CMD_SET_DDRAM | (col + row_offsets[row]));
}

void LCD_Print(const char* str)
{
    while (*str) {
        LCD_SendData(*str++);
    }
}

void LCD_PrintChar(char c)
{
    LCD_SendData(c);
}

void LCD_Backlight(uint8_t on)
{
    backlightState = on ? LCD_BACKLIGHT_ON : LCD_BACKLIGHT_OFF;
    
    // Send dummy byte to update backlight
    uint8_t data = backlightState;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, 100);
}