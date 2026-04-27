/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    buzzer.h
  * @brief   Buzzer control interface
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdint.h>

/* Function prototypes */
void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint32_t duration_ms);
void Buzzer_Pattern(uint8_t count, uint32_t on_ms, uint32_t off_ms);

#endif /* __BUZZER_H__ */