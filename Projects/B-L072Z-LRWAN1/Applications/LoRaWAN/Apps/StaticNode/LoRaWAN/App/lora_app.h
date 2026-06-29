/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.h
  * @author  MCD Application Team
  * @brief   Header for LoRaWAN application layer
  *          - Configuration defines for LoRaWAN stack
  *          - Application settings (TX duty cycle, ports, class)
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LORA_APP_H__
#define __LORA_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

/* ============================================================
 * LoRaWAN Configuration
 * ============================================================ */

/* LoRaWAN region */
#define ACTIVE_REGION                               LORAMAC_REGION_EU868

/* Cayenne LPP payload format (myDevices Application Server) */
#define CAYENNE_LPP    1

/* Transmission duty cycle (ms) */
#define APP_TX_DUTYCYCLE                           10000

/* ============================================================
 * LoRaWAN Ports
 * ============================================================ */

/* User application port (do not use 224 - reserved for certification) */
#define LORAWAN_USER_APP_PORT                       2

/* Switch class application port (do not use 224 - reserved for certification) */
#define LORAWAN_SWITCH_CLASS_PORT                   3

/* ============================================================
 * LoRaWAN Stack Settings
 * ============================================================ */

/* Default device class */
#define LORAWAN_DEFAULT_CLASS                       CLASS_A

/* Default confirmation state */
#define LORAWAN_DEFAULT_CONFIRMED_MSG_STATE         LORAMAC_HANDLER_UNCONFIRMED_MSG

/* Adaptive Data Rate (enable for static devices) */
#define LORAWAN_ADR_STATE                           LORAMAC_HANDLER_ADR_ON

/* Default data rate (used when ADR is disabled) */
#define LORAWAN_DEFAULT_DATA_RATE                   DR_5

/* Default activation type */
#define LORAWAN_DEFAULT_ACTIVATION_TYPE             ACTIVATION_TYPE_OTAA

/* User application data buffer size */
#define LORAWAN_APP_DATA_BUFFER_MAX_SIZE            242

/* Ping slot periodicity: 2^n seconds (n=4 → 16 seconds) */
#define LORAWAN_DEFAULT_PING_SLOT_PERIODICITY       4

/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initialize LoRaWAN application
  */
void LoRaWAN_Init(void);

/**
  * @brief  Restart LoRaWAN stack
  * @note   Called after returning from Raw LoRa mode
  */
void LoRaWAN_Restart(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*__LORA_APP_H__*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/