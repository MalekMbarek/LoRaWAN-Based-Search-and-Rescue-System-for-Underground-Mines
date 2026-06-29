/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    buzzer.h
  * @brief   Buzzer control interface for ScannerNode
  *          - GPIO-based buzzer control
  *          - Simple on/off, single beep, and pattern functions
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdint.h>

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/**
  * @brief  Initialize buzzer GPIO
  */
void Buzzer_Init(void);

/**
  * @brief  Turn buzzer on
  */
void Buzzer_On(void);

/**
  * @brief  Turn buzzer off
  */
void Buzzer_Off(void);

/**
  * @brief  Generate a single beep
  * @param  duration_ms Beep duration in milliseconds
  */
void Buzzer_Beep(uint32_t duration_ms);

/**
  * @brief  Generate a beep pattern
  * @param  count Number of beeps
  * @param  on_ms Beep duration in milliseconds
  * @param  off_ms Silence duration between beeps in milliseconds
  */
void Buzzer_Pattern(uint8_t count, uint32_t on_ms, uint32_t off_ms);

#endif /* __BUZZER_H__ */