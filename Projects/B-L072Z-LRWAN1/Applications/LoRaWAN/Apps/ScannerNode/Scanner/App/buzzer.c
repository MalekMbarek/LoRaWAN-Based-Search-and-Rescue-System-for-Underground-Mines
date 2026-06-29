/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    buzzer.c
  * @brief   Buzzer control implementation for ScannerNode
  *          - GPIO-based buzzer control
  *          - Simple on/off and pattern generation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "buzzer.h"
#include "stm32l0xx_hal.h"

/* ============================================================
 * Buzzer GPIO Configuration
 * ============================================================ */
#define BUZZER_GPIO_PORT        GPIOA
#define BUZZER_GPIO_PIN         GPIO_PIN_8
#define BUZZER_GPIO_CLK_ENABLE  __HAL_RCC_GPIOA_CLK_ENABLE

/* ============================================================
 * Public Functions
 * ============================================================ */

/**
  * @brief  Initialize buzzer GPIO
  * @note   Configures pin as push-pull output, starts in OFF state
  */
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

/**
  * @brief  Turn buzzer on
  */
void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
}

/**
  * @brief  Turn buzzer off
  */
void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  Generate a single beep
  * @param  duration_ms Beep duration in milliseconds
  */
void Buzzer_Beep(uint32_t duration_ms)
{
    Buzzer_On();
    HAL_Delay(duration_ms);
    Buzzer_Off();
}

/**
  * @brief  Generate a beep pattern
  * @param  count Number of beeps
  * @param  on_ms Beep duration in milliseconds
  * @param  off_ms Silence duration between beeps in milliseconds
  */
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