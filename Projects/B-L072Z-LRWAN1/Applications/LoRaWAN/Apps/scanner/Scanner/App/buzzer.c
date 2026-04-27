/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    buzzer.c
  * @brief   Buzzer implementation (GPIO or PWM)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "buzzer.h"
#include "stm32l0xx_hal.h"

/* Buzzer GPIO configuration (example: PA8) */
#define BUZZER_GPIO_PORT        GPIOA
#define BUZZER_GPIO_PIN         GPIO_PIN_8
#define BUZZER_GPIO_CLK_ENABLE  __HAL_RCC_GPIOA_CLK_ENABLE

void Buzzer_Init(void)
{
    BUZZER_GPIO_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);
    
    Buzzer_Off();
}

void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
}

void Buzzer_Beep(uint32_t duration_ms)
{
    Buzzer_On();
    HAL_Delay(duration_ms);
    Buzzer_Off();
}

void Buzzer_Pattern(uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
    for (uint8_t i = 0; i < count; i++) {
        Buzzer_On();
        HAL_Delay(on_ms);
        Buzzer_Off();
        if (i < count - 1) {
            HAL_Delay(off_ms);
        }
    }
}