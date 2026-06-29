/**
  * @file    joystick.h
  * @brief   Joystick driver interface for ScannerNode
  *          - Analog X/Y axis reading
  *          - Digital button (SW) with pull-up
  */
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "stm32l0xx_hal.h"

/* ============================================================
 * Global Variables
 * ============================================================ */
extern uint16_t joy_x;    /* X-axis analog value (0-4095) */
extern uint16_t joy_y;    /* Y-axis analog value (0-4095) */

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/**
  * @brief  Read joystick X and Y analog values
  */
void Read_Joystick(void);

/**
  * @brief  Initialize joystick button (SW) pin
  */
void Joystick_SW_Init(void);

/**
  * @brief  Check if joystick button is pressed
  * @retval 1 if pressed, 0 if released
  */
uint8_t Joystick_SW_Pressed(void);

#endif /* JOYSTICK_H */