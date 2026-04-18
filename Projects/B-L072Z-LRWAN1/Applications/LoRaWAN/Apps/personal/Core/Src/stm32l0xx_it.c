/*
  ******************************************************************************
  * @file         stm32l0xx_it.c
  * @brief       
  */
#include "main.h"
#include "stm32l0xx_it.h"

extern volatile uint8_t sosTriggered;

void NMI_Handler(void)           { while(1); }
void HardFault_Handler(void)     { while(1); }
void MemManage_Handler(void)     { while(1); }
void BusFault_Handler(void)      { while(1); }
void UsageFault_Handler(void)    { while(1); }
void SVC_Handler(void)           { }
void DebugMon_Handler(void)      { }
void PendSV_Handler(void)        { }
void SysTick_Handler(void)       { HAL_IncTick(); }

void EXTI2_3_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_2))
    {
        sosTriggered = 1;
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
    }
}