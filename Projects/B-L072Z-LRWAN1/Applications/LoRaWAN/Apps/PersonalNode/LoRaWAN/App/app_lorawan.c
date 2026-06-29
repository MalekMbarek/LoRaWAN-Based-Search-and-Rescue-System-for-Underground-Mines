/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_lorawan.c — PersonalNode
  * @brief   LoRaWAN application initialization and processing
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app_lorawan.h"
#include "lora_app.h"
#include "sys_app.h"
#include "stm32_seq.h"

/* USER CODE BEGIN Includes */
#include "app_config.h"
#include "stm32l0xx_hal.h"
/* USER CODE END Includes */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/**
  * @brief Initialize LoRaWAN application
  * @note  Calls SystemApp_Init() for system peripherals, then LoRaWAN_Init()
  *        for radio and sensor initialization
  */
void MX_LoRaWAN_Init(void)
{
  /* USER CODE BEGIN MX_LoRaWAN_Init_1 */
  /* USER CODE END MX_LoRaWAN_Init_1 */

  SystemApp_Init();

  /* USER CODE BEGIN MX_LoRaWAN_Init_2 */
  /* USER CODE END MX_LoRaWAN_Init_2 */

  LoRaWAN_Init();

  /* USER CODE BEGIN MX_LoRaWAN_Init_3 */
  /* USER CODE END MX_LoRaWAN_Init_3 */
}

/**
  * @brief Run LoRaWAN application process
  * @note  Executes all registered sequencer tasks (sensors, display, TX)
  */
void MX_LoRaWAN_Process(void)
{
  /* USER CODE BEGIN MX_LoRaWAN_Process_1 */
  /* USER CODE END MX_LoRaWAN_Process_1 */
  UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
  /* USER CODE BEGIN MX_LoRaWAN_Process_2 */
  /* USER CODE END MX_LoRaWAN_Process_2 */
}

/* USER CODE BEGIN PrFD */

/* GPIO initialization moved to sys_app.c */
/*
static void GPIO_PersonalNode_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // PA0 — SOS Button (EXTI falling, pull-up)
  GPIO_InitStruct.Pin  = SOS_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SOS_BUTTON_PORT, &GPIO_InitStruct);
  HAL_NVIC_SetPriority(SOS_BUTTON_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(SOS_BUTTON_IRQn);

  // PA5 — SOS LED
  GPIO_InitStruct.Pin   = LED_SOS_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_SOS_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LED_SOS_PORT, LED_SOS_PIN, GPIO_PIN_RESET);

  // PB13 — Heartbeat LED
  GPIO_InitStruct.Pin = LED_HEARTBEAT_PIN;
  HAL_GPIO_Init(LED_HEARTBEAT_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN, GPIO_PIN_RESET);
}
*/

/* USER CODE END PrFD */