/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c
  * @brief   Mine monitoring sensors: HTS221, LPS22HB, LSM303AGR, MQ2, Buzzer
  *
  * MQ2 wiring (verified working):
  *   AOUT -> PA0  (ADC1 channel 0, 12-bit, 0-3.3V)
  *   VCC  -> 3.3V
  *   GND  -> GND
  *   DOUT -> not connected
  *
  * PLACE IN: Core/Src/sys_sensors.c
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "sys_conf.h"
#include "sys_sensors.h"
#include "sys_app.h"
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "app_config.h"
#include "stm32_timer.h"
#include "stm32l0xx_hal.h"

/* Direct sensor drivers (no IKS01Ax middleware) */
#undef HTS221_I2C_ADDRESS   /* prevent redefinition warning */
#include "hts221.h"
#include "lps22hb.h"

/* B-L072Z-LRWAN1 BSP I2C bus */
#include "b-l072z-lrwan1.h"
#include "b_l072z_lrwan1_bus.h"
/* USER CODE END Includes */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MQ2_ADC_RANK         ADC_RANK_CHANNEL_NUMBER
#define MQ2_ADC_SAMPLINGTIME ADC_SAMPLETIME_39CYCLES_5

#define BUZZER_GPIO_PORT          GPIOB
#define BUZZER_GPIO_PIN           GPIO_PIN_5
#define BUZZER_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()

/* LSM303AGR I2C address -- same as ACC_ADDR in app_config.h (0x32) */
#define ACC_I2C_ADDR   ACC_ADDR
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
EnvData_t    envData;
MotionData_t motionData;
GasData_t    gasData;
uint8_t      mq2_initialized     = 0;
uint8_t      mq2_preheated       = 0;

static HTS221_Object_t   hts221_obj;
static LPS22HB_Object_t  lps22hb_obj;
static uint8_t           acc_initialized    = 0;
static float             mq2_Ro             = 10.0f;  /* kOhm, updated by MQ2_Calibrate */
static ADC_HandleTypeDef AdcHandle;

/* Buzzer: prevents nested/concurrent beeps */
static volatile uint8_t  buzzerActive    = 0;
/* Previous alert levels: buzzer sounds only when level changes */
static uint8_t           last_lvl_motion = 0;
static uint8_t           last_lvl_gas    = 0;

/* MQ2 warmup: HAL_GetTick() value recorded at SensorsInit() */
static uint32_t mq2_preheat_start_ms = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* I2C wrappers for sensor drivers */
static int32_t BSP_I2C_Init  (void) { BSP_I2C1_Init(); return 0; }
static int32_t BSP_I2C_Write (uint16_t addr, uint16_t reg, uint8_t *p, uint16_t len)
{ return BSP_I2C1_WriteReg(addr, reg, p, len); }
static int32_t BSP_I2C_Read  (uint16_t addr, uint16_t reg, uint8_t *p, uint16_t len)
{ return BSP_I2C1_ReadReg(addr, reg, p, len); }

/* ── MQ2 ADC -- PA0 / ADC_CHANNEL_0 ──────────────────────────────────────
 *
 * Why we call HAL_ADC_Stop() before every HAL_ADC_Start():
 *   DualMode switches between LoRaWAN and raw LoRa may restart peripheral
 *   clocks. Calling Stop+Start each time resets the ADC state machine and
 *   prevents HAL_TIMEOUT returns that silently produce 0.
 */
static void MQ2_ADC_Init(void)
{
  /* Configure PA0 as analog input */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin  = MQ2_GPIO_PIN;    /* GPIO_PIN_0 = PA0 */
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MQ2_GPIO_PORT, &gpio);

  /* Configure ADC1 */
  __HAL_RCC_ADC1_CLK_ENABLE();
  AdcHandle.Instance                   = ADC1;
  AdcHandle.Init.OversamplingMode      = DISABLE;
  AdcHandle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
  AdcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
  AdcHandle.Init.SamplingTime          = MQ2_ADC_SAMPLINGTIME;
  AdcHandle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
  AdcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  AdcHandle.Init.ContinuousConvMode    = DISABLE;
  AdcHandle.Init.DiscontinuousConvMode = DISABLE;
  AdcHandle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  AdcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  AdcHandle.Init.DMAContinuousRequests = DISABLE;
  AdcHandle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  AdcHandle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  AdcHandle.Init.LowPowerAutoWait      = DISABLE;
  AdcHandle.Init.LowPowerAutoPowerOff  = DISABLE;

  if (HAL_ADC_Init(&AdcHandle) != HAL_OK)
    APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] HAL_ADC_Init FAILED\r\n");

  /* Self-calibration -- mandatory on STM32L0 before first conversion */
  if (HAL_ADCEx_Calibration_Start(&AdcHandle, ADC_SINGLE_ENDED) != HAL_OK)
    APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Calibration FAILED\r\n");
}

/**
 * @brief Perform a single ADC conversion on MQ2_ADC_CHANNEL (PA0).
 *
 * HAL_ADC_Stop() is called first to ensure the state machine is clean.
 * On conversion timeout the ADC is fully deinitialised and reinitialised.
 *
 * @return 12-bit raw value (0-4095), or 0 on hardware error.
 */
static uint16_t MQ2_ReadADC(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  /* Stop any previous conversion -- harmless if already stopped */
  HAL_ADC_Stop(&AdcHandle);

  /* Re-enable ADC clock: may be gated after low-power or DualMode switch */
  __HAL_RCC_ADC1_CLK_ENABLE();

  sConfig.Channel = MQ2_ADC_CHANNEL;   /* ADC_CHANNEL_0 = PA0 */
  sConfig.Rank    = MQ2_ADC_RANK;
  if (HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK) return 0;

  HAL_ADC_Start(&AdcHandle);

  if (HAL_ADC_PollForConversion(&AdcHandle, 100) == HAL_OK) {
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&AdcHandle);
    HAL_ADC_Stop(&AdcHandle);
    return val;
  }

  /* Timeout: full ADC reinitialisation */
  HAL_ADC_Stop(&AdcHandle);
  HAL_ADC_DeInit(&AdcHandle);
  MQ2_ADC_Init();
  APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] ADC timeout -> reinit\r\n");
  return 0;
}

/**
 * @brief Compute sensor resistance Rs (kOhm) from a raw ADC value.
 *
 * With MQ2 powered at 3.3V and STM32 VREF = 3.3V:
 *   Vadc = (adc_raw / 4095) * 3.3V
 *   Rs   = ((3.3 / Vadc) - 1) * RL
 *
 * @param adc_raw Raw 12-bit ADC value.
 * @return Rs in kOhm (clamped to [0, RL*200]).
 */
static float MQ2_ComputeRs(uint16_t adc_raw)
{
  if (adc_raw == 0) return MQ2_RL_KOHM * 200.0f;  /* open-circuit guard */
  float vadc = ((float)adc_raw / MQ2_ADC_MAX) * MQ2_ADC_VREF;
  if (vadc < 0.01f) return MQ2_RL_KOHM * 200.0f;
  /* VCC == VREF == 3.3V -> simplified formula */
  float rs = ((MQ2_VCC / vadc) - 1.0f) * MQ2_RL_KOHM;
  return (rs < 0.0f) ? 0.0f : rs;
}

/* USER CODE END PFP */

/* ── Buzzer ──────────────────────────────────────────────────────────────── */
void Buzzer_On(void)
{ HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET); }

void Buzzer_Off(void)
{ HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET); }

static void Buzzer_Beep(uint32_t durationMs, uint8_t count)
{
  if (buzzerActive) return;
  buzzerActive = 1;
  for (uint8_t b = 0; b < count; b++) {
    Buzzer_On();
    HAL_Delay(durationMs);
    Buzzer_Off();
    if (b < count - 1) HAL_Delay(80);
  }
  buzzerActive = 0;
}

static void Buzzer_Alert(uint8_t level)
{
  if (buzzerActive) return;
  switch (level) {
    case BUZZER_LEVEL_OK:   Buzzer_Beep(150, 2); break;
    case BUZZER_LEVEL_WARN: Buzzer_Beep(100, 3); break;
    case BUZZER_LEVEL_CRIT: Buzzer_Beep(300, 5); break;
    case BUZZER_LEVEL_SOS:
      for (uint8_t r = 0; r < 3; r++) {
        Buzzer_Beep(200, 1); HAL_Delay(40);
        Buzzer_Beep(200, 1); HAL_Delay(40);
        Buzzer_Beep(300, 1); HAL_Delay(80);
      }
      break;
    default: break;
  }
}

/* ── ST LoRaWAN stack compatibility wrappers ─────────────────────────────── */
int32_t EnvSensors_Init(void) { SensorsInit(); return 0; }

int32_t EnvSensors_Read(sensor_t *sensor_data)
{
  EnvRead();
  if (sensor_data != NULL) {
    sensor_data->temperature = (float)envData.temperature / 10.0f;
    sensor_data->humidity    = (float)envData.humidity    / 10.0f;
    sensor_data->pressure    = (float)envData.pressure    / 10.0f;
    sensor_data->latitude    = 0;
    sensor_data->longitude   = 0;
    sensor_data->altitudeGps = 0;
    sensor_data->altitudeBar = 0;
  }
  return 0;
}

/* ============================================================================
 * SensorsInit -- call once at startup
 * ========================================================================== */
void SensorsInit(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== Sensor initialization ===\r\n");
  BSP_I2C1_Init();

  /* I2C bus scan (diagnostic -- comment out in production) */
  APP_LOG(TS_OFF, VLEVEL_M, "  I2C scan:\r\n");
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    uint8_t dummy = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), 0, 1, &dummy, 1, 10) == HAL_OK)
      APP_LOG(TS_OFF, VLEVEL_M, "    0x%02X found\r\n", addr);
  }

  /* ── Buzzer (PB5) -- startup beep ── */
  BUZZER_GPIO_CLK_ENABLE();
  GPIO_InitTypeDef gpioInit = {0};
  gpioInit.Pin   = BUZZER_GPIO_PIN;
  gpioInit.Mode  = GPIO_MODE_OUTPUT_PP;
  gpioInit.Pull  = GPIO_NOPULL;
  gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_PORT, &gpioInit);
  Buzzer_Off();
  Buzzer_On(); HAL_Delay(1000); Buzzer_Off();   /* 1s startup beep */
  APP_LOG(TS_OFF, VLEVEL_M, "  Buzzer  OK (PB5) -- 1s test done\r\n");

  /* ── HTS221 -- temperature + humidity ── */
  hts221_obj.IO.Init     = BSP_I2C_Init;
  hts221_obj.IO.DeInit   = NULL;
  hts221_obj.IO.BusType  = HTS221_I2C_BUS;
  hts221_obj.IO.Address  = HTS221_I2C_ADDRESS;   /* 0x5F */
  hts221_obj.IO.WriteReg = (HTS221_WriteReg_Func)BSP_I2C_Write;
  hts221_obj.IO.ReadReg  = (HTS221_ReadReg_Func) BSP_I2C_Read;
  hts221_obj.IO.GetTick  = (HTS221_GetTick_Func) BSP_GetTick;
  if (HTS221_RegisterBusIO(&hts221_obj, &hts221_obj.IO) == HTS221_OK &&
      HTS221_Init(&hts221_obj) == HTS221_OK)
  {
    HTS221_TEMP_Enable(&hts221_obj);
    HTS221_HUM_Enable(&hts221_obj);
    HAL_Delay(50);
    hts221_obj.is_initialized = 1;
    APP_LOG(TS_OFF, VLEVEL_M, "  HTS221  OK\r\n");
  }
  else APP_LOG(TS_OFF, VLEVEL_M, "  HTS221  FAILED\r\n");

  /* ── LPS22HB -- barometric pressure ── */
  lps22hb_obj.IO.Init     = BSP_I2C_Init;
  lps22hb_obj.IO.DeInit   = NULL;
  lps22hb_obj.IO.BusType  = LPS22HB_I2C_BUS;
  lps22hb_obj.IO.Address  = LPS22HB_I2C_ADDRESS; /* 0xBB */
  lps22hb_obj.IO.WriteReg = (LPS22HB_WriteReg_Func)BSP_I2C_Write;
  lps22hb_obj.IO.ReadReg  = (LPS22HB_ReadReg_Func) BSP_I2C_Read;
  lps22hb_obj.IO.GetTick  = (LPS22HB_GetTick_Func) BSP_GetTick;
  if (LPS22HB_RegisterBusIO(&lps22hb_obj, &lps22hb_obj.IO) == LPS22HB_OK &&
      LPS22HB_Init(&lps22hb_obj) == LPS22HB_OK)
  {
    LPS22HB_PRESS_Enable(&lps22hb_obj);
    lps22hb_obj.is_initialized = 1;
    APP_LOG(TS_OFF, VLEVEL_M, "  LPS22HB OK\r\n");
  }
  else APP_LOG(TS_OFF, VLEVEL_M, "  LPS22HB FAILED\r\n");

  /* ── LSM303AGR -- accelerometer (direct register access) ── */
  {
    uint8_t who = 0;
    BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_WHO_AM_I, &who, 1);
    APP_LOG(TS_OFF, VLEVEL_M, "  LSM303AGR WHO_AM_I=0x%02X %s\r\n",
            who, (who == 0x33) ? "OK" : "NOT DETECTED");
    if (who == 0x33) {
      uint8_t v1 = ACC_CTRL1_VAL;  /* ODR=100Hz, XYZ on */
      uint8_t v4 = ACC_CTRL4_VAL;  /* BDU=1, FS=±8g, HR=1 */
      BSP_I2C_Write(ACC_I2C_ADDR, ACC_REG_CTRL1, &v1, 1);
      BSP_I2C_Write(ACC_I2C_ADDR, ACC_REG_CTRL4, &v4, 1);
      acc_initialized = 1;
    }
  }

  /* ── MQ2 -- ADC on PA0 ──────────────────────────────────────────────────
   *
   * mq2_preheated starts at 0. GasRead() always reads the ADC so adc_raw
   * is visible in the serial log during warmup, but alert flags stay 0
   * until Sensors_UpdateAll() sets mq2_preheated=1 after MQ2_PREHEAT_MS.
   */
  MQ2_ADC_Init();
  mq2_initialized      = 1;
  mq2_preheated        = 0;
  mq2_preheat_start_ms = HAL_GetTick();

  uint16_t test = MQ2_ReadADC();
  APP_LOG(TS_OFF, VLEVEL_M, "  MQ2     ADC raw=%d on PA0 @3.3V %s\r\n", test,
          (test >= 4090) ? "SATURATED -- check wiring" :
          (test == 0)    ? "ZERO -- check PA0 connection"  : "OK");
  APP_LOG(TS_OFF, VLEVEL_M, "  MQ2     Warmup %ds -- alerts disabled\r\n",
          MQ2_PREHEAT_MS / 1000);

  APP_LOG(TS_OFF, VLEVEL_M, "==============================\r\n\r\n");
}

/* ============================================================================
 * EnvRead -- read T, H, P from HTS221 and LPS22HB into envData (x10 integers)
 * ========================================================================== */
void EnvRead(void)
{
  float temp = 0.0f, hum = 0.0f, pres = 0.0f;
  if (hts221_obj.is_initialized) {
    HTS221_TEMP_GetTemperature(&hts221_obj, &temp);
    HTS221_HUM_GetHumidity    (&hts221_obj, &hum);
    envData.temperature = (int16_t) (temp * 10.0f);
    envData.humidity    = (uint16_t)(hum  * 10.0f);
  }
  if (lps22hb_obj.is_initialized) {
    LPS22HB_PRESS_GetPressure(&lps22hb_obj, &pres);
    envData.pressure = (uint16_t)(pres * 10.0f);
  }
}

/* ============================================================================
 * MotionRead -- read XYZ from LSM303AGR, compute vibration and tilt angles
 * ========================================================================== */
void MotionRead(void)
{
  if (!acc_initialized) return;

  /* Poll ZYXDA (bit3 of STATUS_REG): new XYZ sample ready */
  uint8_t status = 0;
  BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_STATUS, &status, 1);
  if (!(status & 0x08)) return;

  /* Read 6 bytes XYZ -- auto-increment enabled by bit7 of register address */
  uint8_t raw[6] = {0};
  BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_OUT_X_L, raw, 6);

  /* HR mode: 12-bit value left-aligned in 16-bit register -> right-shift 4 */
  int16_t rx = (int16_t)((raw[1] << 8) | raw[0]) >> 4;
  int16_t ry = (int16_t)((raw[3] << 8) | raw[2]) >> 4;
  int16_t rz = (int16_t)((raw[5] << 8) | raw[4]) >> 4;

  motionData.acc_x = (float)rx * ACC_SENSITIVITY_MG;
  motionData.acc_y = (float)ry * ACC_SENSITIVITY_MG;
  motionData.acc_z = (float)rz * ACC_SENSITIVITY_MG;

  /* Vibration = deviation of vector norm from 1g (1000 mg) at rest */
  float norm = sqrtf(motionData.acc_x * motionData.acc_x +
                     motionData.acc_y * motionData.acc_y +
                     motionData.acc_z * motionData.acc_z);
  motionData.vibration            = fabsf(norm - 1000.0f);
  motionData.alert_vibration      = (motionData.vibration > THRESHOLD_VIBRATION_MG);
  motionData.alert_vibration_crit = (motionData.vibration > THRESHOLD_VIBRATION_CRITICAL_MG);

  /* Tilt angles using atan2 -- more stable than asin near 90 deg */
  motionData.angle_x = atan2f(motionData.acc_x,
      sqrtf(motionData.acc_y * motionData.acc_y +
            motionData.acc_z * motionData.acc_z)) * 180.0f / (float)M_PI;
  motionData.angle_y = atan2f(motionData.acc_y,
      sqrtf(motionData.acc_x * motionData.acc_x +
            motionData.acc_z * motionData.acc_z)) * 180.0f / (float)M_PI;
  motionData.angle_total    = sqrtf(motionData.angle_x * motionData.angle_x +
                                    motionData.angle_y * motionData.angle_y);
  motionData.alert_tilt      = (motionData.angle_total > THRESHOLD_TILT_DEG);
  motionData.alert_tilt_crit = (motionData.angle_total > THRESHOLD_TILT_CRITICAL_DEG);
}

/* ============================================================================
 * GasRead -- read MQ2 ADC on PA0, compute Rs/Ro and ppm, set alert flags
 *
 * Design rules:
 *  1. ADC is read on EVERY call (adc_raw always visible in serial log).
 *  2. Alert flags are suppressed while mq2_preheated == 0.
 *  3. HAL_ADC_Stop() before HAL_ADC_Start() prevents state machine lockup.
 * ========================================================================== */
void GasRead(void)
{
  if (!mq2_initialized) return;

  /* Always read raw ADC -- value displayed even during warmup */
  gasData.adc_raw = MQ2_ReadADC();

  /* Compute Rs/Ro and ppm (if ppm mode is configured) */
#if (MQ2_USE_RAW_THRESHOLD == 0)
  float rs = MQ2_ComputeRs(gasData.adc_raw);
  float ratio = rs / mq2_Ro;
  gasData.rs_ro_ratio = ratio;
  ratio = (ratio < 0.01f) ? 0.01f : ratio;
  gasData.ppm = MQ2_CH4_A * powf(ratio, MQ2_CH4_B);
#else
  gasData.rs_ro_ratio = 0.0f;
  gasData.ppm         = 0.0f;
#endif

  /* Suppress alerts during warmup -- only adc_raw is updated */
  if (!mq2_preheated) {
    gasData.alert_gas       = 0;
    gasData.alert_critical  = 0;
    gasData.alert_explosive = 0;
    return;
  }

  /* Set alert flags after warmup */
#if (MQ2_USE_RAW_THRESHOLD == 1)
  gasData.alert_gas       = (gasData.adc_raw > MQ2_RAW_THRESHOLD_WARN);
  gasData.alert_critical  = (gasData.adc_raw > MQ2_RAW_THRESHOLD_CRIT);
  gasData.alert_explosive = (gasData.adc_raw > MQ2_RAW_THRESHOLD_EXPL);
#else
  gasData.alert_gas       = (gasData.ppm > MQ2_THRESHOLD_WARN_PPM);
  gasData.alert_critical  = (gasData.ppm > MQ2_THRESHOLD_CRIT_PPM);
  gasData.alert_explosive = (gasData.ppm > MQ2_THRESHOLD_EXPLOSIVE_PPM);
#endif
}

/* ============================================================================
 * MQ2_Calibrate -- measure Ro in clean air (call once after warmup is done)
 * ========================================================================== */
void MQ2_Calibrate(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Calibrating Ro in clean air...\r\n");
  float rs_sum = 0.0f;
  for (uint8_t i = 0; i < 50; i++) {
    rs_sum += MQ2_ComputeRs(MQ2_ReadADC());
    HAL_Delay(100);
  }
  float rs_mean = rs_sum / 50.0f;
  mq2_Ro = rs_mean / MQ2_RO_CLEAN_AIR_RATIO;
  if (mq2_Ro < 1.0f || mq2_Ro > 100.0f) {
    APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Ro=%.1f out of range -- keeping default 10k\r\n",
            mq2_Ro);
    mq2_Ro = 10.0f;
  } else {
    APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Calibrated Ro=%d.%d kOhm\r\n",
            (int)mq2_Ro,
            (int)((mq2_Ro - (int)mq2_Ro) * 10.0f));
  }
}

/* ============================================================================
 * ProcessAlertes -- drive the buzzer, sounds only when alert level changes
 * ========================================================================== */
void ProcessAlertes(void)
{
  uint8_t lvl_motion = BUZZER_LEVEL_OK;
  if      (motionData.alert_vibration_crit || motionData.alert_tilt_crit)
    lvl_motion = BUZZER_LEVEL_CRIT;
  else if (motionData.alert_vibration || motionData.alert_tilt)
    lvl_motion = BUZZER_LEVEL_WARN;

  uint8_t lvl_gas = BUZZER_LEVEL_OK;
  if      (gasData.alert_explosive) lvl_gas = BUZZER_LEVEL_SOS;
  else if (gasData.alert_critical)  lvl_gas = BUZZER_LEVEL_CRIT;
  else if (gasData.alert_gas)       lvl_gas = BUZZER_LEVEL_WARN;

  /* Print alert summary only when level changes */
  if (lvl_motion != last_lvl_motion || lvl_gas != last_lvl_gas)
    DisplayAlertes();

  if (lvl_motion != last_lvl_motion) {
    last_lvl_motion = lvl_motion;
    if (lvl_motion > BUZZER_LEVEL_OK) Buzzer_Alert(lvl_motion);
  }
  if (lvl_gas != last_lvl_gas) {
    last_lvl_gas = lvl_gas;
    if (lvl_gas > BUZZER_LEVEL_OK) Buzzer_Alert(lvl_gas);
  }
}

/* ============================================================================
 * DisplaySensorData -- single-line serial log + alert lines if active
 * ========================================================================== */
void DisplaySensorData(void)
{
  char     buffer[220];
  uint32_t now = HAL_GetTick() / 1000;

  int16_t  t = envData.temperature;
  uint16_t h = envData.humidity;
  uint16_t p = envData.pressure;

  int32_t vib_i = (int32_t)motionData.vibration;
  int32_t vib_d = (int32_t)((motionData.vibration  - (float)vib_i)  * 10.0f);
  if (vib_d  < 0) vib_d  = -vib_d;

  int32_t tilt_i = (int32_t)motionData.angle_total;
  int32_t tilt_d = (int32_t)((motionData.angle_total - (float)tilt_i) * 10.0f);
  if (tilt_d < 0) tilt_d = -tilt_d;

  int32_t ax_i = (int32_t)motionData.acc_x;
  int32_t ax_d = (int32_t)((motionData.acc_x - (float)ax_i) * 10.0f);
  if (ax_d < 0) ax_d = -ax_d;
  int32_t ay_i = (int32_t)motionData.acc_y;
  int32_t ay_d = (int32_t)((motionData.acc_y - (float)ay_i) * 10.0f);
  if (ay_d < 0) ay_d = -ay_d;
  int32_t az_i = (int32_t)motionData.acc_z;
  int32_t az_d = (int32_t)((motionData.acc_z - (float)az_i) * 10.0f);
  if (az_d < 0) az_d = -az_d;

  snprintf(buffer, sizeof(buffer),
    "[%lu] T=%d.%dC H=%d.%d%% P=%d.%dhPa "
    "ACC=[%ld.%ld,%ld.%ld,%ld.%ld]mg "
    "Vib=%ld.%ldmg Tilt=%ld.%lddeg "
   "Gas=%ldppm ",
    (unsigned long)now,
    t / 10, (t < 0 ? -t : t) % 10,
    h / 10, h % 10,
    p / 10, p % 10,
    (long)ax_i, (long)ax_d,
    (long)ay_i, (long)ay_d,
    (long)az_i, (long)az_d,
    (long)vib_i,  (long)vib_d,
    (long)tilt_i, (long)tilt_d,
    (long)gasData.ppm,
gasData.adc_raw,
    mq2_preheated ? "" : " (warmup)");
  APP_PPRINTF("%s", buffer);

  DisplayAlertes();
}

/* ============================================================================
 * DisplayAlertes -- print active alert lines to serial
 * ========================================================================== */
void DisplayAlertes(void)
{
  char buffer[128];

  if (motionData.alert_vibration_crit) {
    int32_t i = (int32_t)motionData.vibration;
    int32_t d = (int32_t)((motionData.vibration - (float)i) * 10.0f);
    if (d < 0) d = -d;
    snprintf(buffer, sizeof(buffer),
             "  *** CRITICAL VIBRATION: %ld.%ldmg ***\r\n", (long)i, (long)d);
    APP_PPRINTF("%s", buffer);
  } else if (motionData.alert_vibration) {
    int32_t i = (int32_t)motionData.vibration;
    int32_t d = (int32_t)((motionData.vibration - (float)i) * 10.0f);
    if (d < 0) d = -d;
    snprintf(buffer, sizeof(buffer),
             "  [!] Vibration: %ld.%ldmg\r\n", (long)i, (long)d);
    APP_PPRINTF("%s", buffer);
  }

  if (motionData.alert_tilt_crit) {
    int32_t i = (int32_t)motionData.angle_total;
    int32_t d = (int32_t)((motionData.angle_total - (float)i) * 10.0f);
    if (d < 0) d = -d;
    snprintf(buffer, sizeof(buffer),
             "  *** CRITICAL TILT: %ld.%lddeg ***\r\n", (long)i, (long)d);
    APP_PPRINTF("%s", buffer);
  } else if (motionData.alert_tilt) {
    int32_t i = (int32_t)motionData.angle_total;
    int32_t d = (int32_t)((motionData.angle_total - (float)i) * 10.0f);
    if (d < 0) d = -d;
    snprintf(buffer, sizeof(buffer),
             "  [!] Tilt: %ld.%lddeg\r\n", (long)i, (long)d);
    APP_PPRINTF("%s", buffer);
  }

  if (gasData.alert_explosive)
    APP_PPRINTF("  *** SOS GAS raw=%d -- EVACUATE! ***\r\n", (long)gasData.ppm);
  else if (gasData.alert_critical)
    APP_PPRINTF("  *** CRITICAL GAS raw=%d ***\r\n", (long)gasData.ppm);
  else if (gasData.alert_gas)
    APP_PPRINTF("  [!] Gas warning raw=%d\r\n", (long)gasData.ppm);
}

/* ============================================================================
 * Sensors_UpdateAll -- called by AlertTimer every 1 second
 *
 * Warmup completion is checked BEFORE GasRead() so that alert flags are
 * active on the very first call after the 30-second delay.
 * ========================================================================== */
void Sensors_UpdateAll(void)
{
  /* Check warmup -- sets mq2_preheated=1 once MQ2_PREHEAT_MS has elapsed */
  if (!mq2_preheated && mq2_initialized) {
    uint32_t elapsed = HAL_GetTick() - mq2_preheat_start_ms;
    if (elapsed >= MQ2_PREHEAT_MS) {
      mq2_preheated = 1;
      APP_LOG(TS_OFF, VLEVEL_M,
              "[MQ2] Warmup complete (%lus) -- gas alerts now active\r\n",
              (unsigned long)(elapsed / 1000));
#if (MQ2_USE_RAW_THRESHOLD == 0)
      MQ2_Calibrate();
#else
      APP_LOG(TS_OFF, VLEVEL_M,
              "[MQ2] Mode: raw thresholds  warn>%d  crit>%d  expl>%d\r\n",
              MQ2_RAW_THRESHOLD_WARN,
              MQ2_RAW_THRESHOLD_CRIT,
              MQ2_RAW_THRESHOLD_EXPL);
#endif
      Buzzer_Beep(100, 2);   /* two short beeps = warmup done */
    }
  }

  EnvRead();
  MotionRead();
  GasRead();
  ProcessAlertes();
}

/* USER CODE END EF */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/