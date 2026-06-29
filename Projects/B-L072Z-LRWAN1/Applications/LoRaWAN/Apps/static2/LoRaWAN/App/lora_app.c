/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
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

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "Region.h" /* Needed for LORAWAN_DEFAULT_DATA_RATE */
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lora_app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc_if.h"
#include "sys_conf.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include "dual_mode.h"
#include "app_config.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */
static UTIL_TIMER_Object_t ModeSwitchButtonTimer;
static UTIL_TIMER_Object_t AlertTimer;
/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
typedef enum TxEventType_e
{
  TX_ON_TIMER,
  TX_ON_EVENT
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
static void SendTxData(void);
void LoRaWAN_Restart(void);
static void OnTxTimerEvent(void *context);
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);
static void OnTxData(LmHandlerTxParams_t *params);
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);
static void OnMacProcessNotify(void);

/* USER CODE BEGIN PFP */
static void OnTxTimerLedEvent(void *context);
static void OnRxTimerLedEvent(void *context);
static void OnJoinTimerLedEvent(void *context);
static void OnAlertTimerEvent(void *context);
/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

static LmHandlerCallbacks_t LmHandlerCallbacks =
{
  .GetBatteryLevel =           GetBatteryLevel,
  .GetTemperature =            GetTemperatureLevel,
  .GetUniqueId =               GetUniqueId,
  .GetDevAddr =                GetDevAddr,
  .OnMacProcess =              OnMacProcessNotify,
  .OnJoinRequest =             OnJoinRequest,
  .OnTxData =                  OnTxData,
  .OnRxData =                  OnRxData
};

static LmHandlerParams_t LmHandlerParams =
{
  .ActiveRegion =             ACTIVE_REGION,
  .DefaultClass =             LORAWAN_DEFAULT_CLASS,
  .AdrEnable =                LORAWAN_ADR_STATE,
  .TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
  .PingPeriodicity =          LORAWAN_DEFAULT_PING_SLOT_PERIODICITY
};

static TxEventType_t EventType = TX_ON_TIMER;

static UTIL_TIMER_Object_t TxTimer;

/* USER CODE BEGIN PV */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };
static uint8_t AppLedStateOn = RESET;

static UTIL_TIMER_Object_t TxLedTimer;
static UTIL_TIMER_Object_t RxLedTimer;
static UTIL_TIMER_Object_t JoinLedTimer;
/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_1 */
  LED_Init(LED_BLUE);
  LED_Init(LED_RED1);
  LED_Init(LED_RED2);

  APP_LOG(TS_OFF, VLEVEL_M, "APP_VERSION:        V%X.%X.%X\r\n",
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB2_SHIFT));

  APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION: V%X.%X.%X\r\n",
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB2_SHIFT));

  APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:   V%X.%X.%X\r\n",
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB2_SHIFT));

  UTIL_TIMER_Create(&TxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&RxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
  UTIL_TIMER_Create(&JoinLedTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
  UTIL_TIMER_SetPeriod(&TxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&RxLedTimer, 500);
  UTIL_TIMER_SetPeriod(&JoinLedTimer, 500);
  UTIL_TIMER_Create(&ModeSwitchButtonTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, NULL, NULL);

  /* Timer pour mise à jour périodique des capteurs (1 Hz) */
  UTIL_TIMER_Create(&AlertTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnAlertTimerEvent, NULL);
  UTIL_TIMER_SetPeriod(&AlertTimer, 5000);
  UTIL_TIMER_Start(&AlertTimer);

  /* USER CODE END LoRaWAN_Init_1 */

  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), UTIL_SEQ_RFU, SendTxData);

  LoraInfo_Init();
  DualMode_Init();

  /* Initialisation des capteurs (buzzer, MQ2, LSM303AGR, IKS01A2) */
  SensorsInit();
  EnvRead();
  MotionRead();
  GasRead();
  DisplaySensorData();

  /* Init the Lora Stack */
  LmHandlerInit(&LmHandlerCallbacks);
  LmHandlerConfigure(&LmHandlerParams);

  /* USER CODE BEGIN LoRaWAN_Init_2 */
  UTIL_TIMER_Start(&JoinLedTimer);
  /* USER CODE END LoRaWAN_Init_2 */

  LmHandlerJoin(ActivationType);

  if (EventType == TX_ON_TIMER)
  {
    UTIL_TIMER_Create(&TxTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&TxTimer, APP_TX_DUTYCYCLE);
    UTIL_TIMER_Start(&TxTimer);
  }
  else
  {
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);
  }

  /* USER CODE BEGIN LoRaWAN_Init_Last */

  /* USER CODE END LoRaWAN_Init_Last */
}

void LoRaWAN_Restart(void)
{
    LmHandlerStop();
    LmHandlerInit(&LmHandlerCallbacks);
    LmHandlerConfigure(&LmHandlerParams);
    LmHandlerJoin(ActivationType);
}

/* USER CODE BEGIN PB_Callbacks */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case USER_BUTTON_PIN:
      if (DualMode_GetCurrent() == MODE_LORAWAN)
        DualMode_Switch(MODE_RAW_LORA_RX);
      else
        DualMode_Switch(MODE_LORAWAN);
      break;
    default:
      break;
  }
}
/* USER CODE END PB_Callbacks */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */
static void OnAlertTimerEvent(void *context)
{
  /* Mise à jour périodique des capteurs + alertes buzzer (1 Hz)
   * DisplaySensorData est appelé toutes les DISPLAY_INTERVAL_MS/1000 secondes
   * grâce au compteur interne de Sensors_UpdateAll.
   * On appelle DisplaySensorData séparément ici pour conserver
   * le comportement original du fichier.                                    */
  Sensors_UpdateAll();   /* EnvRead + MotionRead + GasRead + ProcessAlertes  */
  DisplaySensorData();   /* Affichage console (log série)                    */
}
/* USER CODE END PrFD */

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
  /* USER CODE BEGIN OnRxData_1 */
  if ((appData != NULL) || (params != NULL))
  {
    LED_On(LED_BLUE);
    UTIL_TIMER_Start(&RxLedTimer);

    static const char *slotStrings[] = { "1", "2", "C", "C Multicast", "B Ping-Slot", "B Multicast Ping-Slot" };

    APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Indication ==========\r\n");
    APP_LOG(TS_OFF, VLEVEL_H, "###### D/L FRAME:%04d | SLOT:%s | PORT:%d | DR:%d | RSSI:%d | SNR:%d\r\n",
            params->DownlinkCounter, slotStrings[params->RxSlot], appData->Port, params->Datarate, params->Rssi, params->Snr);
    switch (appData->Port)
    {
      case LORAWAN_SWITCH_CLASS_PORT:
        if (appData->BufferSize == 1)
        {
          switch (appData->Buffer[0])
          {
            case 0: LmHandlerRequestClass(CLASS_A); break;
            case 1: LmHandlerRequestClass(CLASS_B); break;
            case 2: LmHandlerRequestClass(CLASS_C); break;
            default: break;
          }
        }
        break;
      case LORAWAN_USER_APP_PORT:
        if (appData->BufferSize == 1)
        {
          AppLedStateOn = appData->Buffer[0] & 0x01;
          if (AppLedStateOn == RESET)
          {
            APP_LOG(TS_OFF, VLEVEL_H, "LED OFF\r\n");
            LED_Off(LED_RED1);
          }
          else
          {
            APP_LOG(TS_OFF, VLEVEL_H, "LED ON\r\n");
            LED_On(LED_RED1);
          }
        }
        break;
      default:
        break;
    }
  }
  /* USER CODE END OnRxData_1 */
}

static void SendTxData(void)
{
  /* USER CODE BEGIN SendTxData_1 */
  UTIL_TIMER_Time_t nextTxIn = 0;
  

  /* Mise à jour des capteurs avant envoi */
  EnvRead();
  MotionRead();
  GasRead();
  ProcessAlertes();

  AppData.Port = LORAWAN_USER_APP_PORT;

#ifdef CAYENNE_LPP
  CayenneLppReset();

   * envData stocke des entiers ×10 (ex: temperature=247 → 24.7°C)
   * CayenneLPP attend des valeurs réelles → on divise par 10              */
  CayenneLppAddBarometricPressure(0, (float)envData.pressure    / 10.0f);
  CayenneLppAddTemperature       (1, (float)envData.temperature / 10.0f);
  CayenneLppAddRelativeHumidity  (2, (float)envData.humidity    / 10.0f);

  
  uint8_t alert_motion =
      (motionData.alert_vibration      ? 0x01 : 0) |
      (motionData.alert_tilt           ? 0x02 : 0) |
      (motionData.alert_vibration_crit ? 0x04 : 0) |
      (motionData.alert_tilt_crit      ? 0x08 : 0);
  CayenneLppAddDigitalInput(3, alert_motion);

  CayenneLppAddAnalogInput(1, gasData.ppm);

  uint8_t alert_gas =
      (gasData.alert_gas       ? 0x01 : 0) |
      (gasData.alert_critical  ? 0x02 : 0) |
      (gasData.alert_explosive ? 0x04 : 0);
  CayenneLppAddDigitalInput(4, alert_gas);
  

  StoredPersonalNode_t activeNodes[MAX_PERSONAL_NODES];
  uint8_t activeCount = 0;
  DualMode_GetAllActiveNodes(activeNodes, &activeCount);
static const char* statusStr[] = {"OK", "FALL", "SOS"};
  APP_LOG(TS_OFF, VLEVEL_L, "[TX] Active personal nodes: %d\r\n", activeCount);

  for (uint8_t i = 0; i < activeCount; i++)
{
    StoredPersonalNode_t *n = &activeNodes[i];

    uint8_t nodeId = n->nodeId;
    uint8_t status = n->status;
    uint8_t hr     = n->heartRate;
    uint8_t spo2   = n->spo2;
    uint8_t alert  = n->hasAlert;
float distance = n->distance;
  
    uint8_t baseChannel = 5 + (nodeId - 1) * 5 ;

    CayenneLppAddDigitalInput(baseChannel, nodeId);
    CayenneLppAddDigitalOutput(baseChannel + 1, status);
    CayenneLppAddAnalogInput(baseChannel + 2, hr);
    CayenneLppAddAnalogInput(baseChannel + 3, spo2);
    CayenneLppAddAnalogInput(baseChannel + 4, distance );
  
APP_LOG(TS_OFF, VLEVEL_M,
"[TX] Node%d | status=%d HR=%d SpO2=%d alert=%d Dist=%d.%02d m\r\n",
nodeId,
activeNodes[i].status,
activeNodes[i].heartRate,
activeNodes[i].spo2,
activeNodes[i].hasAlert,
(int)activeNodes[i].distance

);
    if (n->hasAlert)
    {
        DualMode_ClearAlert(nodeId);
    }
}

  CayenneLppCopy(AppData.Buffer);
  AppData.BufferSize = CayenneLppGetSize();

  APP_LOG(TS_OFF, VLEVEL_L, "[TX] Cayenne LPP size: %d bytes\r\n", AppData.BufferSize);

#else  /* not CAYENNE_LPP */
  /* Mode binaire — gardé pour compatibilité */
  uint32_t idx = 0;
  int16_t  temperature = (int16_t)(envData.temperature / 10);
  uint16_t pressure    = (uint16_t)(envData.pressure   / 10);
  uint16_t humidity    = envData.humidity;            /* déjà ×10          */

  AppData.Buffer[idx++] = AppLedStateOn;
  AppData.Buffer[idx++] = (uint8_t)((pressure >> 8) & 0xFF);
  AppData.Buffer[idx++] = (uint8_t)(pressure        & 0xFF);
  AppData.Buffer[idx++] = (uint8_t)(temperature     & 0xFF);
  AppData.Buffer[idx++] = (uint8_t)((humidity >> 8) & 0xFF);
  AppData.Buffer[idx++] = (uint8_t)(humidity        & 0xFF);
  AppData.BufferSize = idx;
#endif /* CAYENNE_LPP */

  if (LORAMAC_HANDLER_SUCCESS == LmHandlerSend(&AppData, LORAWAN_DEFAULT_CONFIRMED_MSG_STATE, &nextTxIn, false))
  {
    APP_LOG(TS_ON, VLEVEL_L, "SEND REQUEST\r\n");
  }
  else if (nextTxIn > 0)
  {
    APP_LOG(TS_ON, VLEVEL_L, "Next Tx in: ~%d second(s)\r\n", (nextTxIn / 1000));
  }

  /* USER CODE END SendTxData_1 */
}

static void OnTxTimerEvent(void *context)
{
  /* USER CODE BEGIN OnTxTimerEvent_1 */

  /* USER CODE END OnTxTimerEvent_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
  UTIL_TIMER_Start(&TxTimer);
  /* USER CODE BEGIN OnTxTimerEvent_2 */

  /* USER CODE END OnTxTimerEvent_2 */
}

/* USER CODE BEGIN PrFD_LedEvents */
static void OnTxTimerLedEvent(void *context)
{
  LED_Off(LED_RED2);
}

static void OnRxTimerLedEvent(void *context)
{
  LED_Off(LED_BLUE);
}

static void OnJoinTimerLedEvent(void *context)
{
  LED_Toggle(LED_RED1);
}
/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
  /* USER CODE BEGIN OnTxData_1 */
  if (params != NULL)
  {
    if (params->IsMcpsConfirm != 0)
    {
      LED_On(LED_RED2);
      UTIL_TIMER_Start(&TxLedTimer);

      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Confirm =============\r\n");
      APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:%04d | PORT:%d | DR:%d | PWR:%d", params->UplinkCounter,
              params->AppData.Port, params->Datarate, params->TxPower);

      APP_LOG(TS_OFF, VLEVEL_H, " | MSG TYPE:");
      if (params->MsgType == LORAMAC_HANDLER_CONFIRMED_MSG)
      {
        APP_LOG(TS_OFF, VLEVEL_H, "CONFIRMED [%s]\r\n", (params->AckReceived != 0) ? "ACK" : "NACK");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_H, "UNCONFIRMED\r\n");
      }
    }
  }
  /* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
  /* USER CODE BEGIN OnJoinRequest_1 */
  if (joinParams != NULL)
  {
    if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
    {
      UTIL_TIMER_Stop(&JoinLedTimer);
      LED_Off(LED_RED1);
      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
      if (joinParams->Mode == ACTIVATION_TYPE_ABP)
      {
        APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
      }
      else
      {
        APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
      }
      DualMode_SetJoinStatus(1);
      
    }
    else
    {
      APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
      DualMode_SetJoinStatus(0);
    }
  }
  /* USER CODE END OnJoinRequest_1 */
}

static void OnMacProcessNotify(void)
{
  /* USER CODE BEGIN OnMacProcessNotify_1 */

  /* USER CODE END OnMacProcessNotify_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);
  /* USER CODE BEGIN OnMacProcessNotify_2 */

  /* USER CODE END OnMacProcessNotify_2 */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/