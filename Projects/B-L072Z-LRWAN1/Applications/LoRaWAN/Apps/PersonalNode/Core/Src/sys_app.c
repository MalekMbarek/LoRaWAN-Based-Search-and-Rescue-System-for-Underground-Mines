/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_app.c — PersonalNode
  *
  * FIX BOUTON SOS :
  *
  * PROBLÈME IDENTIFIÉ :
  *   app_lorawan.c appelait GPIO_PersonalNode_Init() AVANT SystemApp_Init().
  *   SystemApp_Init() → Gpio_PreInit() remettait PA0 en ANALOG après coup.
  *   Résultat : bouton SOS inactif.
  *
  * SOLUTION :
  *   GPIO_PersonalNode_Init() est déclarée ici (sys_app.c) et appelée
  *   DIRECTEMENT APRÈS Gpio_PreInit() dans SystemApp_Init().
  *   app_lorawan.c ne configure PLUS les GPIO — il appelle seulement
  *   SystemApp_Init() qui fait tout dans le bon ordre.
  *
  * ORDRE CORRECT dans SystemApp_Init() :
  *   1. UTIL_TIMER_Init()         ← RTC + timers
  *   2. Gpio_PreInit()            ← tous GPIO → ANALOG
  *   3. GPIO_PersonalNode_Init()  ← PA0/PA5/PB13 reconfigurés APRÈS
  *   4. DBG_Disable/ProbesInit()
  *   5. UART, ADC, SX1276, LPM
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
/*
 * GPIO_PersonalNode_Init : déclarée static ici, appelée dans SystemApp_Init()
 * juste après Gpio_PreInit().
 * NE PAS l'appeler depuis app_lorawan.c.
 */
static void GPIO_PersonalNode_Init(void);
/* USER CODE END PFP */

/* ════════════════════════════════════════════════════════════════════════════
 * SystemApp_Init
 * ════════════════════════════════════════════════════════════════════════════ */
void SystemApp_Init(void)
{
  /* USER CODE BEGIN SystemApp_Init_1 */
  /* USER CODE END SystemApp_Init_1 */

  /* 1. Init RTC + timers */
  UTIL_TIMER_Init();

  /* 2. Gpio_PreInit : remet TOUS les GPIO en ANALOG (y compris PA0/PB8/PB9) */
  Gpio_PreInit();

  /* 3. GPIO_PersonalNode_Init : reconfigure PA0 (SOS), PA5 (LED), PB13 (LED)
   *    IMMÉDIATEMENT APRÈS Gpio_PreInit() pour éviter l'écrasement.
   *    PB8/PB9 (I2C) : laissés en ANALOG, reconfigurés par I2C_BusRecovery +
   *    PN_I2C_Init() dans SensorsInit() (sys_sensors.c).
   */
  GPIO_PersonalNode_Init();

  /* 4. Debug */
  DBG_Disable();
  DBG_ProbesInit();

  /* 5. UART logs */
  UTIL_ADV_TRACE_Init();
  UTIL_ADV_TRACE_RegisterTimeStampFunction(TimestampNow);
  UTIL_ADV_TRACE_SetVerboseLevel(VERBOSE_LEVEL);

  /* 6. ADC + SX1276 */
  SYS_InitMeasurement();
  Sx_Board_Bus_Init();
  Sx_Board_IoInit();

  /* 7. LPM */
  UTIL_LPM_Init();
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 1)
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
#elif !defined (LOW_POWER_DISABLE)
#error LOW_POWER_DISABLE not defined
#endif

  /* USER CODE BEGIN SystemApp_Init_2 */
  /* USER CODE END SystemApp_Init_2 */
}

/* ════════════════════════════════════════════════════════════════════════════
 * GPIO_PersonalNode_Init
 *
 * Appelée DANS SystemApp_Init(), juste après Gpio_PreInit().
 * Reconfigure seulement les GPIO du nœud personnel :
 *   PA0 : bouton SOS (EXTI falling, pull-up)
 *   PA5 : LED SOS    (output PP)
 *   PB13: LED heartbeat (output PP)
 *
 * PB8/PB9 (I2C) : NE PAS configurer ici.
 *   sys_sensors.c les configure via I2C_BusRecovery() + PN_I2C_Init().
 * ════════════════════════════════════════════════════════════════════════════ */
static void GPIO_PersonalNode_Init(void)
{
  GPIO_InitTypeDef g = {0};

  /* Réactiver les horloges GPIO (Gpio_PreInit les avait désactivées) */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* ── PA0 — Bouton SOS ─────────────────────────────────────────────────
   * Gpio_PreInit() avait remis PA0 en ANALOG → on le reconfigure ici.
   * Mode : EXTI falling edge (bouton appuyé = GND = LOW).
   * Pull : pull-up interne (relâché = PA0 = HIGH).
   */
  g.Pin  = SOS_BUTTON_PIN;           /* GPIO_PIN_0 */
  g.Mode = GPIO_MODE_IT_FALLING;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SOS_BUTTON_PORT, &g); /* GPIOA */

  /* Effacer le flag EXTI résiduel AVANT d'activer l'IRQ
   * (évite une fausse interruption au démarrage) */
  __HAL_GPIO_EXTI_CLEAR_IT(SOS_BUTTON_PIN);

  /* Priorité 2 : en dessous de 0/1 réservés noyau, au-dessus de 3+ applicatifs */
  HAL_NVIC_SetPriority(SOS_BUTTON_IRQn, 2, 0);  /* EXTI0_1_IRQn */
  HAL_NVIC_EnableIRQ(SOS_BUTTON_IRQn);

  /* ── PA5 — LED SOS ──────────────────────────────────────────────────── */
  g.Pin   = LED_SOS_PIN;              /* GPIO_PIN_5 */
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_SOS_PORT, &g);    /* GPIOA */
  HAL_GPIO_WritePin(LED_SOS_PORT, LED_SOS_PIN, GPIO_PIN_RESET);

  /* ── PB13 — LED Heartbeat ───────────────────────────────────────────── */
  g.Pin = LED_HEARTBEAT_PIN;          /* GPIO_PIN_13 */
  HAL_GPIO_Init(LED_HEARTBEAT_PORT, &g); /* GPIOB */
  HAL_GPIO_WritePin(LED_HEARTBEAT_PORT, LED_HEARTBEAT_PIN, GPIO_PIN_RESET);
}

/* ════════════════════════════════════════════════════════════════════════════
 * UTIL_SEQ_Idle — entre en low power quand aucune tâche n'est prête
 * ════════════════════════════════════════════════════════════════════════════ */
void UTIL_SEQ_Idle(void)
{
  UTIL_LPM_EnterLowPower();
}

/* ════════════════════════════════════════════════════════════════════════════
 * GetBatteryLevel / GetTemperatureLevel / GetUniqueId / GetDevAddr
 * ════════════════════════════════════════════════════════════════════════════ */
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

uint16_t GetTemperatureLevel(void)
{
  return (uint16_t)(SYS_GetTemperatureLevel() / 256);
}

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

uint32_t GetDevAddr(void)
{
  return HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Private functions
 * ════════════════════════════════════════════════════════════════════════════ */
static void TimestampNow(uint8_t *buff, uint16_t *size)
{
  SysTime_t curtime = SysTimeGet();
  tiny_snprintf_like((char *)buff, MAX_TS_SIZE, "%ds%03d:",
                     curtime.Seconds, curtime.SubSeconds);
  *size = (uint16_t)strlen((char *)buff);
}

static void Gpio_PreInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  /* GPIOA : tous sauf PA13/PA14 (SWD/SWCLK debug) */
  GPIO_InitStruct.Pin = GPIO_PIN_All & (~(GPIO_PIN_13 | GPIO_PIN_14));
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* GPIOB/C/H : tous */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
}

/* Disable StopMode when traces need to be printed */
void UTIL_ADV_TRACE_PreSendHook(void)
{
  UTIL_LPM_SetStopMode((1 << CFG_LPM_UART_TX_Id), UTIL_LPM_DISABLE);
}

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
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  /* SysTick désactivé — on utilise le RTC comme base de temps */
  return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
  return RTC_IF_GetTimerValue();
}

void HAL_Delay(__IO uint32_t Delay)
{
  RTC_IF_DelayMs(Delay);
}