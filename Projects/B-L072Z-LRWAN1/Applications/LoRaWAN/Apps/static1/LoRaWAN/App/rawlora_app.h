/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rawlora_app.h
  * @author  Custom Implementation
  * @brief   Header for Raw LoRa application logic
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RAWLORA_APP_H__
#define __RAWLORA_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "radio.h"

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  uint8_t Buffer[255];
  uint8_t BufferSize;
  int16_t Rssi;
  int8_t Snr;
} RawLoRaRxData_t;

/* Exported constants --------------------------------------------------------*/
#define RAWLORA_RX_TIMEOUT_MS    5000  /* 5 seconds receive window */
#define RAWLORA_FREQUENCY        868000000  /* 868 MHz */
#define RAWLORA_TX_POWER         14
#define RAWLORA_BANDWIDTH        0      /* 0: 125 kHz */
#define RAWLORA_SPREADING_FACTOR 7      /* SF7 */
#define RAWLORA_CODING_RATE      1      /* 4/5 */
#define RAWLORA_PREAMBLE_LENGTH  8
#define RAWLORA_FIXED_LENGTH     false
#define RAWLORA_CRC_ON           false
#define RAWLORA_FREQ_HOP_ON      false
#define RAWLORA_HOP_PERIOD       0
#define RAWLORA_IQ_INVERSION     false 
#define RAWLORA_RX_CONTINUOUS    true
//#define RAWLORA_RX_DURATION_MS   5000


/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initialize Raw LoRa receiver
  * @param  None
  * @retval None
  */
void RawLoRa_Init(void);

/**
  * @brief  Process Raw LoRa reception
  * @param  None
  * @retval None
  */
void RawLoRa_Process(void);

/**
  * @brief  Deinitialize Raw LoRa receiver
  * @param  None
  * @retval None
  */
void RawLoRa_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __RAWLORA_APP_H__ */