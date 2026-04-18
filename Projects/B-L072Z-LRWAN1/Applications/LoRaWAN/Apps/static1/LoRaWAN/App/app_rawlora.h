/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_rawlora.h
  * @author  Custom Implementation
  * @brief   Header for Raw LoRa reception functionality
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __APP_RAWLORA_H__
#define __APP_RAWLORA_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Init Raw LoRa Application
  * @param  None
  * @retval None
  */
void MX_RawLoRa_Init(void);

/**
  * @brief  Process Raw LoRa Application (receive frames)
  * @param  None
  * @retval None
  */
void MX_RawLoRa_Process(void);

/**
  * @brief  DeInit Raw LoRa Application
  * @param  None
  * @retval None
  */
void MX_RawLoRa_DeInit(void);

/**
  * @brief  DeInit LoRaWAN Application (to be added to app_lorawan)
  * @param  None
  * @retval None
  */
void MX_LoRaWAN_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RAWLORA_H__ */