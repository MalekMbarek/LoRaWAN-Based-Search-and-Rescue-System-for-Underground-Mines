/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.h — PersonalNode
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __LORA_APP_H__
#define __LORA_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include "app_config.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void LoRaWAN_Init  (void);

/* USER CODE BEGIN EFP */
void    LoRa_Send     (uint8_t status);
uint8_t LoRa_LastTxOk (void);

/* Appelée depuis HAL_GPIO_EXTI_Callback dans main.c */
void    PN_SOS_Trigger(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*__LORA_APP_H__*/