/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lcd_display.c
  * @brief   LCD 20x4 I2C display driver for ScannerNode
  *          - I2C communication with PCF8574 I/O expander
  *          - 4-bit mode operation
  *          - Backlight control
  *          - Cursor positioning and printing
  ******************************************************************************
  */
/* USER CODE END Header */

#include "lcd_display.h"
#include "stm32l0xx_hal.h"
#include <string.h>

/* ============================================================
 * LCD Configuration
 * ============================================================ */

/* I2C LCD address (typically 0x27 or 0x3F) */
#define LCD_I2C_ADDR    0x27 << 1  /* Shifted for HAL (8-bit address) */

/* LCD commands */
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x06
#define LCD_CMD_DISPLAY_ON      0x0C
#define LCD_CMD_DISPLAY_OFF     0x08
#define LCD_CMD_FUNCTION_SET    0x28  /* 4-bit, 2 lines, 5x8 font */
#define LCD_CMD_SET_DDRAM       0x80

/* Backlight control */
#define LCD_BACKLIGHT_ON        0x08
#define LCD_BACKLIGHT_OFF       0x00

/* Timing */
#define LCD_DELAY_MS            5

/* ============================================================
 * Private Variables
 * ============================================================ */

extern I2C_HandleTypeDef hi2c1;
static uint8_t backlightState = LCD_BACKLIGHT_ON;

/* ============================================================
 * Private Function Prototypes
 * ============================================================ */

static void LCD_SendNibble(uint8_t nibble, uint8_t rs);
static void LCD_SendByte(uint8_t byte, uint8_t rs);
static void LCD_SendCommand(uint8_t cmd);
static void LCD_SendData(uint8_t data);

/* ============================================================
 * Private Functions
 * ============================================================ */

/**
  * @brief  Send a 4-bit nibble to LCD
  * @param  nibble 4-bit data to send (upper nibble used)
  * @param  rs Register select (0 = command, 1 = data)
  * @note   Generates EN pulse for each transmission
  */
static void LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = nibble | rs | backlightState;
    
    /* Send with EN high then low */
    uint8_t buf[4];
    buf[0] = data | 0x04;  /* EN = 1 */
    buf[1] = data | 0x04;  /* EN = 1 */
    buf[2] = data & ~0x04; /* EN = 0 */
    buf[3] = data & ~0x04; /* EN = 0 */
    
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, buf, 4, 100);
    HAL_Delay(1);
}

/**
  * @brief  Send a full byte to LCD
  * @param  byte Byte to send
  * @param  rs Register select (0 = command, 1 = data)
  */
static void LCD_SendByte(uint8_t byte, uint8_t rs)
{
    LCD_SendNibble(byte & 0xF0, rs);        /* High nibble */
    LCD_SendNibble((byte << 4) & 0xF0, rs); /* Low nibble */
}

/**
  * @brief  Send a command to LCD
  * @param  cmd Command byte
  */
static void LCD_SendCommand(uint8_t cmd)
{
    LCD_SendByte(cmd, 0);  /* RS = 0 for command */
    HAL_Delay(LCD_DELAY_MS);
}

/**
  * @brief  Send data to LCD
  * @param  data Data byte to display
  */
static void LCD_SendData(uint8_t data)
{
    LCD_SendByte(data, 0x01);  /* RS = 1 for data */
}

/* ============================================================
 * Public Functions
 * ============================================================ */

/**
  * @brief  Initialize LCD in 4-bit mode
  * @note   Requires 50ms power-up delay before initialization
  */
void LCD_Init(void)
{
    HAL_Delay(50);  /* Wait for LCD power-up */
    
    /* Initialize in 4-bit mode */
    LCD_SendNibble(0x30, 0);
    HAL_Delay(5);
    LCD_SendNibble(0x30, 0);
    HAL_Delay(1);
    LCD_SendNibble(0x30, 0);
    HAL_Delay(1);
    LCD_SendNibble(0x20, 0);  /* Switch to 4-bit mode */
    HAL_Delay(1);
    
    /* Function set: 4-bit, 2 lines, 5x8 font */
    LCD_SendCommand(LCD_CMD_FUNCTION_SET);
    
    /* Display on, cursor off, blink off */
    LCD_SendCommand(LCD_CMD_DISPLAY_ON);
    
    /* Clear display */
    LCD_Clear();
    
    /* Entry mode: increment, no shift */
    LCD_SendCommand(LCD_CMD_ENTRY_MODE);
    
    HAL_Delay(10);
}

/**
  * @brief  Clear LCD display
  */
void LCD_Clear(void)
{
    LCD_SendCommand(LCD_CMD_CLEAR);
    HAL_Delay(2);
}

/**
  * @brief  Set cursor position
  * @param  row Row position (0-3 for 4-line display)
  * @param  col Column position (0-19 for 20-column display)
  */
void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};  /* 20x4 LCD */
    
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    
    LCD_SendCommand(LCD_CMD_SET_DDRAM | (col + row_offsets[row]));
}

/**
  * @brief  Print a string on LCD at current cursor position
  * @param  str Null-terminated string to display
  */
void LCD_Print(const char* str)
{
    while (*str) {
        LCD_SendData(*str++);
    }
}

/**
  * @brief  Print a single character on LCD
  * @param  c Character to display
  */
void LCD_PrintChar(char c)
{
    LCD_SendData(c);
}

/**
  * @brief  Control LCD backlight
  * @param  on 1 to turn on, 0 to turn off
  */
void LCD_Backlight(uint8_t on)
{
    backlightState = on ? LCD_BACKLIGHT_ON : LCD_BACKLIGHT_OFF;
    
    uint8_t data = backlightState;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_ADDR, &data, 1, 100);
}