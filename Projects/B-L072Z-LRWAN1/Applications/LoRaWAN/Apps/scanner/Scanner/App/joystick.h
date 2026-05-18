#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "stm32l0xx_hal.h"

extern uint16_t joy_x;
extern uint16_t joy_y;

void Read_Joystick(void);

void Joystick_SW_Init(void);
uint8_t Joystick_SW_Pressed(void);

#endif