/**
  * @file    joystick.c
  * @brief   Joystick driver for ScannerNode
  *          - Analog X/Y axis reading via ADC
  *          - Digital button (SW) with pull-up
  */
#include "joystick.h"
#include "adc.h"
#include "main.h"

/* ============================================================
 * Private Variables
 * ============================================================ */
uint16_t joy_x = 0;
uint16_t joy_y = 0;

/* ============================================================
 * Joystick Analog Functions
 * ============================================================ */

/**
  * @brief  Read joystick X and Y analog values
  * @note   Uses ADC channels 0 (X-axis) and 2 (Y-axis)
  */
void Read_Joystick(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    /* Read X-axis (PA0 - ADC_CHANNEL_0) */
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, 10);
    joy_x = HAL_ADC_GetValue(&hadc);

    /* Read Y-axis (PA2 - ADC_CHANNEL_2) */
    sConfig.Channel = ADC_CHANNEL_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, 10);
    joy_y = HAL_ADC_GetValue(&hadc);

    HAL_ADC_Stop(&hadc);
}

/* ============================================================
 * Joystick Button Functions
 * ============================================================ */

#define JOY_SW_PIN   GPIO_PIN_3
#define JOY_SW_PORT  GPIOA

/**
  * @brief  Initialize joystick button (SW) pin
  * @note   PA3 configured as input with internal pull-up
  */
void Joystick_SW_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = JOY_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(JOY_SW_PORT, &GPIO_InitStruct);
}

/**
  * @brief  Check if joystick button is pressed
  * @retval 1 if pressed, 0 if released
  */
uint8_t Joystick_SW_Pressed(void)
{
    return (HAL_GPIO_ReadPin(JOY_SW_PORT, JOY_SW_PIN) == GPIO_PIN_RESET);
}