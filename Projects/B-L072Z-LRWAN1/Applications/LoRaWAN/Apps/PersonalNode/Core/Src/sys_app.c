/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_app.c — PersonalNode
  * @brief   System application layer with GPIO, battery, and low-power management
  *
  * @note    GPIO_PersonalNode_Init() is called AFTER Gpio_PreInit() in
  *          SystemApp_Init() to ensure proper pin configuration order.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "platform.h"
#include "sys_app.h"
#include "adc_if.h"
#include "radio_board_if.h"
#include "stm32_seq.h"
#include "stm32_systime.h"
#include "stm32_lpm.h"
#include "utilities_def.h"
#include "sys_debug.h"
#include "rtc_if.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "app_config.h"
#include "stm32l0xx_hal.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
#define MAX_TS_SIZE     (int)16
#define LORAWAN_MAX_BAT 254

/* Private function prototypes -----------------------------------------------*/
static void Gpio_PreInit       (void);
static void TimestampNow       (uint8_t *buff, uint16_t *size);
static void tiny_snprintf_like (char *buf, uint32_t maxsize,
                                 const char *strFormat, ...);

/* USER CODE BEGIN PFP */
static void GPIO_PersonalNode_Init(void);
/* USER CODE END PFP */

/**
  * @brief Initialize the system application
  * @note  Called once at startup. Configures timers, GPIO, debug, UART, ADC,
  *        radio bus, and low-power management.
  */
void SystemApp_Init(void)
{
  UTIL_TIMER_Init();
  Gpio_PreInit();
  DBG_Disable();
  DBG_ProbesInit();
  UTIL_ADV_TRACE_Init();
  UTIL_ADV_TRACE_RegisterTimeStampFunction(TimestampNow);
  UTIL_ADV_TRACE_SetVerboseLevel(VERBOSE_LEVEL);
  SYS_InitMeasurement();
  Sx_Board_Bus_Init();
  Sx_Board_IoInit();
  GPIO_PersonalNode_Init();
  UTIL_LPM_Init();
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 1)
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
#elif !defined (LOW_POWER_DISABLE)
#error LOW_POWER_DISABLE not defined
#endif
}

/**
  * @brief Initialize PersonalNode-specific GPIO pins
  * @note  PA5 - SOS LED, PB13 - Heartbeat LED
  *        PB2 (SOS button) is configured by the Semtech BSP in lora_app.c
  */
static void GPIO_PersonalNode_Init(void)
{
  GPIO_InitTypeDef g = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* PA5 - SOS LED */
  g.Pin   = GPIO_PIN_5;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &g);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /* PB13 - Heartbeat LED */
  g.Pin   = GPIO_PIN_13;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
}

/**
  * @brief Idle callback - enters low power mode when no tasks are ready
  */
void UTIL_SEQ_Idle(void)
{
  UTIL_LPM_EnterLowPower();
}

/**
  * @brief Get battery level as a percentage of max (254)
  * @return Battery level from 0 to 254
  */
uint8_t GetBatteryLevel(void)
{
  uint8_t  batteryLevel   = 0;
  uint16_t batteryLevelmV = (uint16_t)SYS_GetBatteryLevel();

  if      (batteryLevelmV > VDD_BAT) batteryLevel = LORAWAN_MAX_BAT;
  else if (batteryLevelmV < VDD_MIN) batteryLevel = 0;
  else
    batteryLevel = (uint8_t)(((uint32_t)(batteryLevelmV - VDD_MIN) *
                               LORAWAN_MAX_BAT) / (VDD_BAT - VDD_MIN));

  APP_LOG(TS_ON, VLEVEL_M, "VDDA= %d\r\n", batteryLevel);
  return batteryLevel;
}

/**
  * @brief Get temperature level
  * @return Temperature value scaled by 1/256
  */
uint16_t GetTemperatureLevel(void)
{
  return (uint16_t)(SYS_GetTemperatureLevel() / 256);
}

/**
  * @brief Get unique device ID (8 bytes)
  * @param id Output buffer for 8-byte unique ID
  */
void GetUniqueId(uint8_t *id)
{
  uint32_t ID_1_3_val = HAL_GetUIDw0() + HAL_GetUIDw2();
  uint32_t ID_2_val   = HAL_GetUIDw1();
  id[7] = (uint8_t)(ID_1_3_val >> 24);
  id[6] = (uint8_t)(ID_1_3_val >> 16);
  id[5] = (uint8_t)(ID_1_3_val >> 8);
  id[4] = (uint8_t)(ID_1_3_val);
  id[3] = (uint8_t)(ID_2_val   >> 24);
  id[2] = (uint8_t)(ID_2_val   >> 16);
  id[1] = (uint8_t)(ID_2_val   >> 8);
  id[0] = (uint8_t)(ID_2_val);
}

/**
  * @brief Get device address (32-bit XOR of UID words)
  * @return 32-bit device address
  */
uint32_t GetDevAddr(void)
{
  return HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
}

/* ============================================================
 * Private functions
 * ============================================================ */

/**
  * @brief Generate timestamp string for trace logs
  */
static void TimestampNow(uint8_t *buff, uint16_t *size)
{
  SysTime_t curtime = SysTimeGet();
  tiny_snprintf_like((char *)buff, MAX_TS_SIZE, "%ds%03d:",
                     curtime.Seconds, curtime.SubSeconds);
  *size = (uint16_t)strlen((char *)buff);
}

/**
  * @brief Pre-initialize all GPIOs to analog mode
  * @note  PA13 and PA14 (SWD/SWCLK) are preserved for debugging
  */
static void Gpio_PreInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  /* GPIOA: all except PA13/PA14 (SWD/SWCLK debug pins) */
  GPIO_InitStruct.Pin = GPIO_PIN_All & (~(GPIO_PIN_13 | GPIO_PIN_14));
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* GPIOB/C/H: all pins */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
}

/**
  * @brief Disable stop mode before UART trace transmission
  */
void UTIL_ADV_TRACE_PreSendHook(void)
{
  UTIL_LPM_SetStopMode((1 << CFG_LPM_UART_TX_Id), UTIL_LPM_DISABLE);
}

/**
  * @brief Re-enable stop mode after UART trace transmission
  */
void UTIL_ADV_TRACE_PostSendHook(void)
{
  UTIL_LPM_SetStopMode((1 << CFG_LPM_UART_TX_Id), UTIL_LPM_ENABLE);
}

static void tiny_snprintf_like(char *buf, uint32_t maxsize,
                                const char *strFormat, ...)
{
  va_list vaArgs;
  va_start(vaArgs, strFormat);
  UTIL_ADV_TRACE_VSNPRINTF(buf, maxsize, strFormat, vaArgs);
  va_end(vaArgs);
}

/* HAL overload functions ----------------------------------------------------*/

/**
  * @brief HAL tick initialization - RTC used instead of SysTick
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  return HAL_OK;
}

/**
  * @brief Get tick value from RTC
  */
uint32_t HAL_GetTick(void)
{
  return RTC_IF_GetTimerValue();
}

/**
  * @brief Delay using RTC timer
  */
void HAL_Delay(__IO uint32_t Delay)
{
  RTC_IF_DelayMs(Delay);
}