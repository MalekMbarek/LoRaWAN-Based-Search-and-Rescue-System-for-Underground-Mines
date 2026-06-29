/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c — PersonalNode
  * @brief   LoRaWAN application layer with sensor reading, SOS, fall detection,
  *          and periodic transmission
  ******************************************************************************
  */
/* USER CODE END Header */

#include "platform.h"
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "stm32_lpm.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "radio.h"
#include "app_config.h"
#include "stm32l0xx_hal.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
/* USER CODE END Includes */

typedef enum {
  PN_RADIO_IDLE = 0,
  PN_RADIO_TX_DONE,
  PN_RADIO_TX_TIMEOUT
} PnRadioState_t;

static void OnTxDone    (void);
static void OnTxTimeout (void);
static void OnRxDone    (uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnRxTimeout (void);
static void OnRxError   (void);
static void OnAccTimer    (void *context);
static void OnHrTimer     (void *context);
static void OnDisplayTimer(void *context);
static void OnTxTimer     (void *context);
static void SosTask  (void);
static void FallTask (void);
static void TxTask   (void);
static void Fall_Update(float ax, float ay, float az);
static void LED_Blink(GPIO_TypeDef *port, uint16_t pin, uint8_t count, uint32_t ms);

static volatile PnRadioState_t radioState = PN_RADIO_IDLE;
static uint8_t                 lastTxOk   = 0;
static RadioEvents_t           RadioEvents;

static UTIL_TIMER_Object_t AccTimer;
static UTIL_TIMER_Object_t HrTimer;
static UTIL_TIMER_Object_t DisplayTimer;
static UTIL_TIMER_Object_t TxTimer;

static uint8_t          inFreefall   = 0;
static uint32_t         freefallTick = 0;
static uint8_t          fallPending  = 0;
volatile uint8_t        sosPending   = 0;
static uint8_t          sosReady     = 0;

/**
  * @brief Initialize LoRaWAN application
  * @note  Configures radio, sensors, GPIO (SOS button), and periodic timers
  */
void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_1 */

  APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== BOOT OK ===\r\n");

  APP_LOG(TS_OFF, VLEVEL_M, "PersonalNode %d -- %uMHz SF%d BW125\r\n",
          (int)MY_NODE_ID,
          (unsigned int)(PN_RF_FREQUENCY / 1000000UL),
          (int)PN_SPREADING_FACTOR);

  /* Radio events */
  RadioEvents.TxDone    = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone    = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError   = OnRxError;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(PN_RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, PN_TX_OUTPUT_POWER, 0,
                    PN_BANDWIDTH, PN_SPREADING_FACTOR, PN_CODINGRATE,
                    PN_PREAMBLE_LENGTH, false, true, 0, 0, false,
                    PN_TX_TIMEOUT_MS);
  APP_LOG(TS_OFF, VLEVEL_M, "SX1276 ready\r\n");

  /* Disable stop mode during sensor initialization */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);

  SensorsInit();

  /* SOS Button PB2 — configured after all other GPIO initializations */
  {
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Pin  = SOS_BUTTON_PIN;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(SOS_BUTTON_PORT, &g);

    HAL_NVIC_DisableIRQ(SOS_BUTTON_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(SOS_BUTTON_PIN);
    HAL_NVIC_SetPriority(SOS_BUTTON_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SOS_BUTTON_IRQn);
  }

  /* Initial sensor reading and display */
  AccRead();
  HrRead();
  DisplaySensorData();

  /* Register sequencer tasks */
  UTIL_SEQ_RegTask(1 << CFG_SEQ_Task_SOS,  UTIL_SEQ_RFU, SosTask);
  UTIL_SEQ_RegTask(1 << CFG_SEQ_Task_FALL, UTIL_SEQ_RFU, FallTask);
  UTIL_SEQ_RegTask(1 << CFG_SEQ_Task_TX,   UTIL_SEQ_RFU, TxTask);

  /* Create periodic timers */
  UTIL_TIMER_Create(&AccTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnAccTimer, NULL);
  UTIL_TIMER_SetPeriod(&AccTimer, PN_ACC_READ_PERIOD_MS);
  UTIL_TIMER_Start(&AccTimer);

  UTIL_TIMER_Create(&HrTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnHrTimer, NULL);
  UTIL_TIMER_SetPeriod(&HrTimer, PN_HR_READ_PERIOD_MS);
  UTIL_TIMER_Start(&HrTimer);

  UTIL_TIMER_Create(&DisplayTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnDisplayTimer, NULL);
  UTIL_TIMER_SetPeriod(&DisplayTimer, PN_PRINT_PERIOD_MS);
  UTIL_TIMER_Start(&DisplayTimer);

  UTIL_TIMER_Create(&TxTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnTxTimer, NULL);
  UTIL_TIMER_SetPeriod(&TxTimer, PN_TX_PERIOD_MS);
  UTIL_TIMER_Start(&TxTimer);

  APP_LOG(TS_OFF, VLEVEL_M,
          "Running -- TX every %ds, Display every %ds\r\n\r\n",
          (int)(PN_TX_PERIOD_MS / 1000),
          (int)(PN_PRINT_PERIOD_MS / 1000));

  /* First transmission immediately */
  UTIL_SEQ_SetTask(1 << CFG_SEQ_Task_TX, CFG_SEQ_Prio_0);
  HAL_Delay(1000);
  sosReady = 1;

  /* USER CODE END LoRaWAN_Init_1 */
}

/* USER CODE BEGIN PrFD */

/* Radio callbacks */
static void OnTxDone(void)    { Radio.Sleep(); radioState = PN_RADIO_TX_DONE;    }
static void OnTxTimeout(void) { Radio.Sleep(); radioState = PN_RADIO_TX_TIMEOUT; }
static void OnRxDone(uint8_t *p, uint16_t s, int16_t r, int8_t n)
{ (void)p;(void)s;(void)r;(void)n; }
static void OnRxTimeout(void) {}
static void OnRxError  (void) {}

/**
  * @brief Periodic accelerometer timer callback
  * @note  Reads accelerometer, updates fall detection, triggers fall task if detected
  */
static void OnAccTimer(void *context)
{
  (void)context;
  if (!acc_present) return;
  AccRead();
  Fall_Update(accData.acc_x, accData.acc_y, accData.acc_z);
  if (fallPending) {
    fallPending = 0;
    UTIL_SEQ_SetTask(1 << CFG_SEQ_Task_FALL, CFG_SEQ_Prio_0);
  }
}

/**
  * @brief Periodic heart rate timer callback
  * @note  Reads MAX30102 HR and SpO2 data
  */
static void OnHrTimer(void *context)
{
  (void)context;
  if (!hr_present) return;
  HrRead();
}

/**
  * @brief Periodic display timer callback
  * @note  Toggles heartbeat LED and displays sensor data
  */
static void OnDisplayTimer(void *context)
{
  (void)context;
  HAL_GPIO_TogglePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN);
  DisplaySensorData();
}

/**
  * @brief Periodic transmission timer callback
  * @note  Triggers TX task
  */
static void OnTxTimer(void *context)
{
  (void)context;
  UTIL_SEQ_SetTask(1 << CFG_SEQ_Task_TX, CFG_SEQ_Prio_0);
}

/**
  * @brief SOS task handler
  * @note  Called when SOS button is pressed
  */
static void SosTask(void)
{
  if (sosPending)
  {
    sosPending = 0;

    APP_LOG(TS_OFF, VLEVEL_M, "!!! SOS ALERT !!!\r\n");

    LED_Blink(LED_SOS_PORT, LED_SOS_PIN, 6, 100);

    AccRead();
    HrRead();

    LoRa_Send(STATUS_SOS);
  }
}

/**
  * @brief Fall detection task handler
  * @note  Called when a fall is detected
  */
static void FallTask(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "!!! FALL DETECTED !!!\r\n");
  LED_Blink(LED_SOS_PORT, LED_SOS_PIN, 10, 80);
  AccRead(); HrRead();
  LoRa_Send(STATUS_FALL);
}

/**
  * @brief Periodic transmission task
  */
static void TxTask(void) { LoRa_Send(STATUS_OK); }

/**
  * @brief Update fall detection state
  * @param ax X-axis acceleration in mg
  * @param ay Y-axis acceleration in mg
  * @param az Z-axis acceleration in mg
  */
static void Fall_Update(float ax, float ay, float az)
{
  float tvm2 = ax*ax + ay*ay + az*az;
  const float ff2  = PN_FREE_FALL_THRESHOLD_MG * PN_FREE_FALL_THRESHOLD_MG;
  const float imp2 = PN_IMPACT_THRESHOLD_MG    * PN_IMPACT_THRESHOLD_MG;

  if (!inFreefall) {
    if (tvm2 < ff2) { inFreefall = 1; freefallTick = HAL_GetTick(); }
  } else {
    if (tvm2 > imp2) {
      fallPending = 1; inFreefall = 0;
    } else if ((HAL_GetTick() - freefallTick) > PN_FALL_WINDOW_MS) {
      inFreefall = 0;
    }
  }
}

/**
  * @brief Blink an LED
  * @param port GPIO port
  * @param pin GPIO pin
  * @param count Number of blinks
  * @param ms On/off duration in milliseconds
  */
static void LED_Blink(GPIO_TypeDef *port, uint16_t pin, uint8_t count, uint32_t ms)
{
  for (uint8_t i = 0; i < count; i++) {
    HAL_GPIO_TogglePin(port, pin); HAL_Delay(ms);
  }
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

/**
  * @brief Send LoRa packet with sensor data
  * @param status Packet status (OK, FALL, SOS)
  */
void LoRa_Send(uint8_t status)
{
  uint8_t payload[PKT_SIZE];
  payload[PKT_I_TYPE]   = PKT_TYPE_FULL;
  payload[PKT_I_ID]     = (uint8_t)MY_NODE_ID;
  payload[PKT_I_STATUS] = status;
  payload[PKT_I_BPM]    = !hr_present      ? 255U : hrData.bpm_valid ? hrData.bpm : 0U;
  payload[PKT_I_AX_H]   = (uint8_t)((uint16_t)accData.raw_x >> 8);
  payload[PKT_I_AX_L]   = (uint8_t)(accData.raw_x & 0xFF);
  payload[PKT_I_AY_H]   = (uint8_t)((uint16_t)accData.raw_y >> 8);
  payload[PKT_I_AY_L]   = (uint8_t)(accData.raw_y & 0xFF);
  payload[PKT_I_AZ_H]   = (uint8_t)((uint16_t)accData.raw_z >> 8);
  payload[PKT_I_AZ_L]   = (uint8_t)(accData.raw_z & 0xFF);
  payload[PKT_I_SPO2]   = hrData.spo2_valid ? hrData.spo2 : 0U;

  const char *st[] = {"OK", "FALL", "SOS"};
  APP_LOG(TS_ON, VLEVEL_M,
          "[LoRa] TX node=%d status=%s HR:%s(%dbpm) SpO2:%s(%d%%)"
          " | X:%d Y:%d Z:%d\r\n",
          (int)MY_NODE_ID, status < 3U ? st[status] : "?",
          !hr_present      ? "N/A" : (hrData.bpm_valid  ? "OK" : "--"),
          (int)payload[PKT_I_BPM],
          !hr_present      ? "N/A" : (hrData.spo2_valid ? "OK" : "--"),
          (int)payload[PKT_I_SPO2],
          (int)accData.raw_x, (int)accData.raw_y, (int)accData.raw_z);

  radioState = PN_RADIO_IDLE;
  Radio.Send(payload, PKT_SIZE);

  uint32_t deadline = HAL_GetTick() + (uint32_t)PN_TX_TIMEOUT_MS + 500U;
  while (radioState == PN_RADIO_IDLE && HAL_GetTick() < deadline) { __NOP(); }

  if (radioState == PN_RADIO_TX_DONE) {
    APP_LOG(TS_ON, VLEVEL_L, "[LoRa] TX done\r\n");
    lastTxOk = 1;
  } else {
    APP_LOG(TS_ON, VLEVEL_L, "[LoRa] TX timeout\r\n");
    lastTxOk = 0;
  }
}

/**
  * @brief Get status of last transmission
  * @return 1 if last TX succeeded, 0 if failed
  */
uint8_t LoRa_LastTxOk(void) { return lastTxOk; }

/**
  * @brief GPIO EXTI interrupt callback for SOS button
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == SOS_BUTTON_PIN && sosReady)
  {
    sosPending = 1;
    UTIL_SEQ_SetTask(1 << CFG_SEQ_Task_SOS, CFG_SEQ_Prio_0);
  }
}

/* USER CODE END PrFD */