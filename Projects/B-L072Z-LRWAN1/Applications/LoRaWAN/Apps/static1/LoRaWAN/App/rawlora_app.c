/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rawlora_app.c
  * @author  Custom Implementation
  * @brief   Raw LoRa application for receiving frames from other nodes
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "rawlora_app.h"
#include "sys_app.h"
#include "radio.h"
#include "stm32_timer.h"
#include "utilities_def.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static RadioEvents_t RadioEvents;
static RawLoRaRxData_t RxData;
static volatile bool FrameReceived = false;
static UTIL_TIMER_Object_t RxTimeoutTimer;
static uint32_t RxStartTime = 0;
static uint32_t ProcessCallCount = 0;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnRxTimeout(void);
static void OnRxError(void);
static void OnRxTimeoutEvent(void *context);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void RawLoRa_Init(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== RAW LORA INIT ==========\r\n");
  
  /* ===== 1. Setup callbacks FIRST ===== */
  RadioEvents.TxDone = NULL;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxError = OnRxError;
  RadioEvents.TxTimeout = NULL;
  RadioEvents.RxTimeout = OnRxTimeout;

  /* ===== 2. Init radio ===== */
  Radio.Sleep();
  HAL_Delay(20);
  Radio.Init(&RadioEvents);

  /* ===== 3. Continue config ===== */
  FrameReceived = false;
  ProcessCallCount = 0;

  Radio.SetChannel(RAWLORA_FREQUENCY);

  Radio.SetRxConfig(MODEM_LORA, RAWLORA_BANDWIDTH, RAWLORA_SPREADING_FACTOR,
                    RAWLORA_CODING_RATE, 0, RAWLORA_PREAMBLE_LENGTH,
                    RAWLORA_RX_TIMEOUT_MS, RAWLORA_FIXED_LENGTH,
                    0, RAWLORA_CRC_ON, RAWLORA_FREQ_HOP_ON,
                    RAWLORA_HOP_PERIOD, RAWLORA_IQ_INVERSION,
                    RAWLORA_RX_CONTINUOUS);

  UTIL_TIMER_Create(&RxTimeoutTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, 
                    OnRxTimeoutEvent, NULL);
  UTIL_TIMER_SetPeriod(&RxTimeoutTimer, RAWLORA_RX_TIMEOUT_MS);

  APP_LOG(TS_OFF, VLEVEL_M, "Radio Status before RX: %d\r\n", Radio.GetStatus());
  
  /* Start timestamp */
  RxStartTime = HAL_GetTick();

  /* Start reception */
  Radio.Rx((uint32_t) (0)); // 0 = continuous RX
  UTIL_TIMER_Start(&RxTimeoutTimer);

  //APP_LOG(TS_OFF, VLEVEL_M, "Radio Status after RX: %d\r\n", Radio.GetStatus());
  int i=5;
  i=i*15+6;
  APP_LOG(TS_OFF, VLEVEL_M, "i =%d \r\n", i);

  APP_LOG(TS_OFF, VLEVEL_M, "Raw LoRa: Listening for frames...\r\n");
  APP_LOG(TS_OFF, VLEVEL_M, "i =%d \r\n", ++i);


  APP_LOG(TS_OFF, VLEVEL_M, "Frequency: %lu Hz\r\n", RAWLORA_FREQUENCY);
  APP_LOG(TS_OFF, VLEVEL_M, "SF: %d, BW: %d, CR: %d\r\n", RAWLORA_SPREADING_FACTOR, RAWLORA_BANDWIDTH, RAWLORA_CODING_RATE);
}

void RawLoRa_Process(void)
{
  ProcessCallCount++;
  
  /* Log every 1000 calls to show we're alive */
  if (ProcessCallCount % 1000 == 0)
  {
    APP_LOG(TS_OFF, VLEVEL_L, "RawLoRa_Process called %lu times, time: %lu ms\r\n", 
            ProcessCallCount, HAL_GetTick() - RxStartTime);
    APP_LOG(TS_OFF, VLEVEL_L, "Radio Status: %d\r\n", Radio.GetStatus());
  }
      APP_LOG(TS_OFF, VLEVEL_L, "BAAD EL IF");

  /* Process radio IRQs - THIS IS CRITICAL */
  Radio.IrqProcess();
        APP_LOG(TS_OFF, VLEVEL_L, "BAAD EL IrqProcess");

  /* Check if frame was received */
  if (FrameReceived)
  {
    FrameReceived = false;
    
    APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### RAW LORA FRAME RECEIVED ######\r\n");
    APP_LOG(TS_OFF, VLEVEL_H, "Size: %d | RSSI: %d | SNR: %d\r\n", 
            RxData.BufferSize, RxData.Rssi, RxData.Snr);
    APP_LOG(TS_OFF, VLEVEL_H, "Data: ");
    for (uint8_t i = 0; i < RxData.BufferSize; i++)
    {
      APP_LOG(TS_OFF, VLEVEL_H, "%02X ", RxData.Buffer[i]);
    }
    APP_LOG(TS_OFF, VLEVEL_H, "\r\n");
    
    /* Restart reception */
    Radio.Rx(0);
    UTIL_TIMER_Start(&RxTimeoutTimer);
    RxStartTime = HAL_GetTick(); // Reset timer
  }
  
  /* Safety check - if we've been running too long without receiving, log it */
  uint32_t elapsed = HAL_GetTick() - RxStartTime;
  if (elapsed > 10000 && ProcessCallCount % 5000 == 0) // Every 10 seconds
  {
    APP_LOG(TS_OFF, VLEVEL_M, "No frame received for %lu ms\r\n", elapsed);
  }
}

void RawLoRa_DeInit(void)
{  
  APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== RAW LORA DEINIT ==========\r\n");
  APP_LOG(TS_OFF, VLEVEL_M, "Total Process calls: %lu\r\n", ProcessCallCount);
  APP_LOG(TS_OFF, VLEVEL_M, "Total time in RX mode: %lu ms\r\n", HAL_GetTick() - RxStartTime);

  Radio.Sleep();
  UTIL_TIMER_Stop(&RxTimeoutTimer);
  
  APP_LOG(TS_OFF, VLEVEL_M, "Radio mode after sleep: %d\r\n", Radio.GetStatus());
}

/* Private functions ---------------------------------------------------------*/

static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
  APP_LOG(TS_OFF, VLEVEL_M, ">>> IRQ RX DONE TRIGGERED <<<\r\n");
  
  /* Copy received data */
  RxData.BufferSize = (size > 255) ? 255 : size;
  memcpy(RxData.Buffer, payload, RxData.BufferSize);
  RxData.Rssi = rssi;
  RxData.Snr = snr;
  
  /* Set flag */
  FrameReceived = true;
  
  /* Stop timeout timer */
  UTIL_TIMER_Stop(&RxTimeoutTimer);
}

static void OnRxTimeout(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, ">>> IRQ RX TIMEOUT TRIGGERED <<<\r\n");
  
  /* Restart reception */
  Radio.Rx(0);
  UTIL_TIMER_Start(&RxTimeoutTimer);
}

static void OnRxError(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, ">>> IRQ RX ERROR TRIGGERED <<<\r\n");
  
  /* Restart reception */
  Radio.Rx(0);
  UTIL_TIMER_Start(&RxTimeoutTimer);
}

static void OnRxTimeoutEvent(void *context)
{
  APP_LOG(TS_OFF, VLEVEL_M, ">>> TIMER TIMEOUT EVENT <<<\r\n");
  APP_LOG(TS_OFF, VLEVEL_M, "No frames received in %d ms\r\n", RAWLORA_RX_TIMEOUT_MS);
  
  /* Optionally restart */
  // Radio.Rx(0);
  // UTIL_TIMER_Start(&RxTimeoutTimer);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/