/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c
  * @brief   Mine monitoring sensors (HTS221, LPS22HB, LSM303AGR, MQ2) + buzzer
  *
  * PLACE IN: Core/Src/sys_sensors.c
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "sys_conf.h"
#include "sys_sensors.h"
#include "sys_debug.h"
#include "sys_app.h"
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "app_config.h"
#include "stm32_timer.h"
#include "stm32l0xx_hal.h"

/* Sensor drivers */
#undef HTS221_I2C_ADDRESS
#include "hts221.h"
#include "lps22hb.h"

/* BSP bus (I2C) */
#include "b-l072z-lrwan1.h"
#include "b_l072z_lrwan1_bus.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MQ2_ADC_CHANNEL             ADC_CHANNEL_4
#define MQ2_ADC_RANK                ADC_RANK_CHANNEL_NUMBER
#define MQ2_ADC_SAMPLINGTIME        ADC_SAMPLETIME_39CYCLES_5

#define BUZZER_GPIO_PORT            GPIOB
#define BUZZER_GPIO_PIN             GPIO_PIN_5
#define BUZZER_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()

#define ACC_I2C_ADDR                ACC_ADDR   /* 0x32 from app_config.h */

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
EnvData_t    envData;
MotionData_t motionData;
GasData_t    gasData;
uint8_t      mq2_initialized = 0;
uint8_t      mq2_preheated   = 0;

static HTS221_Object_t  hts221_obj;
static LPS22HB_Object_t lps22hb_obj;
static uint8_t          acc_initialized = 0;
static float            mq2_Ro = 10.0f;

static ADC_HandleTypeDef AdcHandle;

static volatile uint8_t buzzerActive   = 0;
static uint8_t          last_lvl_motion = 0;
static uint8_t          last_lvl_gas    = 0;

/* Gestion manuelle du préchauffage MQ2 (remplace UTIL_TIMER) */
static uint32_t mq2_preheat_start_ms = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static int32_t BSP_I2C_Init   ( void ) { BSP_I2C1_Init(); return 0; }
static int32_t BSP_I2C_Write  ( uint16_t addr, uint16_t reg, uint8_t *p, uint16_t len ) { return BSP_I2C1_WriteReg(addr, reg, p, len); }
static int32_t BSP_I2C_Read   ( uint16_t addr, uint16_t reg, uint8_t *p, uint16_t len ) { return BSP_I2C1_ReadReg(addr, reg, p, len); }

static void MQ2_ADC_Init( void )
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

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
    HAL_ADC_Init(&AdcHandle);
    HAL_ADCEx_Calibration_Start(&AdcHandle, ADC_SINGLE_ENDED);
}

static uint16_t MQ2_ReadADC( void )
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = MQ2_ADC_CHANNEL;
    sConfig.Rank    = MQ2_ADC_RANK;
    HAL_ADC_ConfigChannel(&AdcHandle, &sConfig);
    HAL_ADC_Start(&AdcHandle);
    if( HAL_ADC_PollForConversion(&AdcHandle, 10) == HAL_OK )
        return (uint16_t)HAL_ADC_GetValue(&AdcHandle);
    return 0;
}

static float MQ2_ComputeRs(uint16_t adc_raw)
{
    if (adc_raw == 0) return MQ2_RL_KOHM * 200.0f;

    float vadc = ((float)adc_raw / MQ2_ADC_MAX) * MQ2_ADC_VREF;
    float vout = vadc * (MQ2_VCC / MQ2_ADC_VREF);
    if (vout < 0.01f) return MQ2_RL_KOHM * 200.0f;

    float rs = ((MQ2_VCC / vout) - 1.0f) * MQ2_RL_KOHM;
    if (rs < 0.0f) rs = 0.0f;
    return rs;
}

static void Buzzer_Beep( uint32_t durationMs, uint8_t count )
{
    if( buzzerActive ) return;
    buzzerActive = 1;
    for( uint8_t b = 0; b < count; b++ ) {
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
        HAL_Delay(durationMs);
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
        if( b < count - 1 ) HAL_Delay(80);
    }
    buzzerActive = 0;
}

static void Buzzer_Alert( uint8_t level )
{
    if( buzzerActive ) return;
    switch( level ) {
    case BUZZER_LEVEL_OK:   Buzzer_Beep(150, 2); break;
    case BUZZER_LEVEL_WARN: Buzzer_Beep(100, 3); break;
    case BUZZER_LEVEL_CRIT: Buzzer_Beep(300, 5); break;
    case BUZZER_LEVEL_SOS:
        for( uint8_t r = 0; r < 3; r++ ) {
            Buzzer_Beep(200, 1); HAL_Delay(40);
            Buzzer_Beep(200, 1); HAL_Delay(40);
            Buzzer_Beep(300, 1); HAL_Delay(80);
        }
        break;
    default: break;
    }
}
/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_Init( void )
{
    SensorsInit();
    return 0;
}

int32_t EnvSensors_Read( sensor_t *sensor_data )
{
    EnvRead();
    if( sensor_data ) {
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

/* USER CODE BEGIN EF */

void SensorsInit( void )
{
    APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== Sensor initialization ===\r\n");

    /* Initialisation I2C */
    BSP_I2C1_Init();

    /* Scan I2C (diagnostic) */
    APP_LOG(TS_OFF, VLEVEL_M, "  I2C scan:\r\n");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t dummy = 0;
        if (HAL_I2C_Mem_Read(&hi2c1, (addr<<1), 0, 1, &dummy, 1, 10) == HAL_OK) {
            APP_LOG(TS_OFF, VLEVEL_M, "    Device found at 0x%02X\r\n", addr);
        }
    }

    /* Buzzer (test 1s) */
    BUZZER_GPIO_CLK_ENABLE();
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin   = BUZZER_GPIO_PIN;
    gpioInit.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull  = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &gpioInit);
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
    APP_LOG(TS_OFF, VLEVEL_M, "  Buzzer test (1s) done\r\n");

    /* HTS221 */
    hts221_obj.IO.Init     = BSP_I2C_Init;
    hts221_obj.IO.DeInit   = NULL;
    hts221_obj.IO.BusType  = HTS221_I2C_BUS;
    hts221_obj.IO.Address  = HTS221_I2C_ADDRESS;  /* 0x5F */
    hts221_obj.IO.WriteReg = (HTS221_WriteReg_Func) BSP_I2C_Write;
    hts221_obj.IO.ReadReg  = (HTS221_ReadReg_Func)  BSP_I2C_Read;
    hts221_obj.IO.GetTick  = (HTS221_GetTick_Func)  BSP_GetTick;

    if( HTS221_RegisterBusIO(&hts221_obj, &hts221_obj.IO) == HTS221_OK &&
        HTS221_Init(&hts221_obj) == HTS221_OK )
    {
        HTS221_TEMP_Enable(&hts221_obj);
        HTS221_HUM_Enable(&hts221_obj);
        HAL_Delay(50);
        hts221_obj.is_initialized = 1;
        APP_LOG(TS_OFF, VLEVEL_M, "  HTS221  OK\r\n");
    }
    else APP_LOG(TS_OFF, VLEVEL_M, "  HTS221  FAILED\r\n");

    /* LPS22HB */
    lps22hb_obj.IO.Init     = BSP_I2C_Init;
    lps22hb_obj.IO.DeInit   = NULL;
    lps22hb_obj.IO.BusType  = LPS22HB_I2C_BUS;
    lps22hb_obj.IO.Address  = LPS22HB_I2C_ADDRESS; /* 0x5D */
    lps22hb_obj.IO.WriteReg = (LPS22HB_WriteReg_Func) BSP_I2C_Write;
    lps22hb_obj.IO.ReadReg  = (LPS22HB_ReadReg_Func)  BSP_I2C_Read;
    lps22hb_obj.IO.GetTick  = (LPS22HB_GetTick_Func)  BSP_GetTick;

    if( LPS22HB_RegisterBusIO(&lps22hb_obj, &lps22hb_obj.IO) == LPS22HB_OK &&
        LPS22HB_Init(&lps22hb_obj) == LPS22HB_OK )
    {
        LPS22HB_PRESS_Enable(&lps22hb_obj);
        lps22hb_obj.is_initialized = 1;
        APP_LOG(TS_OFF, VLEVEL_M, "  LPS22HB OK\r\n");
    }
    else APP_LOG(TS_OFF, VLEVEL_M, "  LPS22HB FAILED\r\n");

    /* LSM303AGR */
    {
        uint8_t who = 0;
        BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_WHO_AM_I, &who, 1);
        APP_LOG(TS_OFF, VLEVEL_M, "  LSM303AGR WHO_AM_I=0x%02X %s\r\n",
                who, (who == 0x33) ? "OK" : "NOT DETECTED");
        if( who == 0x33 ) {
            uint8_t v1 = ACC_CTRL1_VAL, v4 = ACC_CTRL4_VAL;
            BSP_I2C_Write(ACC_I2C_ADDR, ACC_REG_CTRL1, &v1, 1);
            BSP_I2C_Write(ACC_I2C_ADDR, ACC_REG_CTRL4, &v4, 1);
            acc_initialized = 1;
        }
    }

    /* MQ2 (gaz) – préchauffage manuel */
    MQ2_ADC_Init();
    mq2_initialized = 1;
    mq2_preheat_start_ms = HAL_GetTick();   /* début du compte à rebours */
    uint16_t test = MQ2_ReadADC();
    APP_LOG(TS_OFF, VLEVEL_M, "  MQ2     ADC raw=%d %s\r\n", test,
           (test >= 4090) ? "SATURATED" : (test == 0) ? "ZERO -- check wiring" : "OK");
    APP_LOG(TS_OFF, VLEVEL_M, "  MQ2     Warm-up (%ds)...\r\n", MQ2_PREHEAT_MS/1000);

    /* Le timer UTIL_TIMER n'est pas fiable dans ce contexte, on utilise une gestion manuelle */
    /* Les deux lignes suivantes sont commentées :
       UTIL_TIMER_Create(&Mq2PreheatTimer, MQ2_PREHEAT_MS, UTIL_TIMER_ONESHOT, Mq2PreheatCallback, NULL);
       UTIL_TIMER_Start(&Mq2PreheatTimer);
    */

    APP_LOG(TS_OFF, VLEVEL_M, "==============================\r\n\r\n");
}

/* Callback conservé mais non utilisé (remplacé par gestion manuelle) */
static void Mq2PreheatCallback(void *context)
{
    mq2_preheated = 1;
    APP_LOG(TS_OFF, VLEVEL_M, "  [MQ2] Preheat complete.\r\n");
#if (MQ2_USE_RAW_THRESHOLD == 0)
    MQ2_Calibrate();
    APP_LOG(TS_OFF, VLEVEL_M, "  [MQ2] Ready (ppm mode)\r\n");
#else
    APP_LOG(TS_OFF, VLEVEL_M, "  [MQ2] Ready (raw threshold mode)\r\n");
#endif
}

void EnvRead( void )
{
    float temp = 0.0f, hum = 0.0f, pres = 0.0f;
    if( hts221_obj.is_initialized ) {
        HTS221_TEMP_GetTemperature(&hts221_obj, &temp);
        HTS221_HUM_GetHumidity(&hts221_obj, &hum);
        envData.temperature = (int16_t)(temp * 10.0f);
        envData.humidity    = (uint16_t)(hum  * 10.0f);
    }
    if( lps22hb_obj.is_initialized ) {
        LPS22HB_PRESS_GetPressure(&lps22hb_obj, &pres);
        envData.pressure = (uint16_t)(pres * 10.0f);
    }
}

void MotionRead( void )
{
    if( !acc_initialized ) return;

    uint8_t status = 0;
    BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_STATUS, &status, 1);
    if( !(status & 0x08) ) return;

    uint8_t raw[6] = {0};
    BSP_I2C_Read(ACC_I2C_ADDR, ACC_REG_OUT_X_L, raw, 6);

    int16_t rx = (int16_t)((raw[1] << 8) | raw[0]) >> 4;
    int16_t ry = (int16_t)((raw[3] << 8) | raw[2]) >> 4;
    int16_t rz = (int16_t)((raw[5] << 8) | raw[4]) >> 4;

    motionData.acc_x = (float)rx * ACC_SENSITIVITY_MG;
    motionData.acc_y = (float)ry * ACC_SENSITIVITY_MG;
    motionData.acc_z = (float)rz * ACC_SENSITIVITY_MG;

    float norm = sqrtf(motionData.acc_x*motionData.acc_x +
                       motionData.acc_y*motionData.acc_y +
                       motionData.acc_z*motionData.acc_z);
    motionData.vibration            = fabsf(norm - 1000.0f);
    motionData.alert_vibration      = (motionData.vibration > THRESHOLD_VIBRATION_MG);
    motionData.alert_vibration_crit = (motionData.vibration > THRESHOLD_VIBRATION_CRITICAL_MG);

    motionData.angle_x = atan2f(motionData.acc_x,
        sqrtf(motionData.acc_y*motionData.acc_y + motionData.acc_z*motionData.acc_z))
        * 180.0f / (float)M_PI;
    motionData.angle_y = atan2f(motionData.acc_y,
        sqrtf(motionData.acc_x*motionData.acc_x + motionData.acc_z*motionData.acc_z))
        * 180.0f / (float)M_PI;
    motionData.angle_total     = sqrtf(motionData.angle_x*motionData.angle_x +
                                        motionData.angle_y*motionData.angle_y);
    motionData.alert_tilt      = (motionData.angle_total > THRESHOLD_TILT_DEG);
    motionData.alert_tilt_crit = (motionData.angle_total > THRESHOLD_TILT_CRITICAL_DEG);
}

void GasRead( void )
{
    if( !mq2_initialized ) return;

    /* Pendant le préchauffage, on ne lit pas l'ADC pour économiser */
    if( !mq2_preheated ) {
        gasData.adc_raw = 0;
        gasData.ppm = 0;
        gasData.alert_gas = 0;
        gasData.alert_critical = 0;
        gasData.alert_explosive = 0;
        return;
    }

    gasData.adc_raw = MQ2_ReadADC();

#if (MQ2_USE_RAW_THRESHOLD == 1)
    gasData.ppm = 0.0f;
    gasData.alert_gas       = (gasData.adc_raw > MQ2_RAW_THRESHOLD_WARN);
    gasData.alert_critical  = (gasData.adc_raw > MQ2_RAW_THRESHOLD_CRIT);
    gasData.alert_explosive = (gasData.adc_raw > MQ2_RAW_THRESHOLD_EXPL);
#else
    float rs = MQ2_ComputeRs(gasData.adc_raw);
    float ratio = rs / mq2_Ro;
    gasData.rs_ro_ratio = ratio;
    if( ratio < 0.01f ) ratio = 0.01f;
    gasData.ppm = MQ2_CH4_A * powf(ratio, MQ2_CH4_B);
    gasData.alert_gas       = (gasData.ppm > MQ2_THRESHOLD_WARN_PPM);
    gasData.alert_critical  = (gasData.ppm > MQ2_THRESHOLD_CRIT_PPM);
    gasData.alert_explosive = (gasData.ppm > MQ2_THRESHOLD_EXPLOSIVE_PPM);
#endif
}

void MQ2_Calibrate(void)
{
    float rs_sum = 0.0f;
    for (uint8_t i = 0; i < 50; i++) {
        rs_sum += MQ2_ComputeRs(MQ2_ReadADC());
        HAL_Delay(100);
    }
    float rs_mean = rs_sum / 50.0f;
    mq2_Ro = rs_mean / MQ2_RO_CLEAN_AIR_RATIO;
    if (mq2_Ro < 1.0f || mq2_Ro > 100.0f) mq2_Ro = 10.0f;
    APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Calibrated Ro=%.2f kohm\r\n", mq2_Ro);
}

/* ================== AFFICHAGE ================== */
void DisplaySensorData( void )
{
    char buffer[200];
    uint32_t now = HAL_GetTick() / 1000;

    int16_t t = envData.temperature;
    int16_t h = envData.humidity;
    int16_t p = envData.pressure;

    /* Vibration */
    int32_t vib_int = (int32_t)motionData.vibration;
    int32_t vib_dec = (int32_t)((motionData.vibration - vib_int) * 10);
    if (vib_dec < 0) vib_dec = -vib_dec;

    /* Inclinaison */
    int32_t tilt_int = (int32_t)motionData.angle_total;
    int32_t tilt_dec = (int32_t)((motionData.angle_total - tilt_int) * 10);
    if (tilt_dec < 0) tilt_dec = -tilt_dec;

    /* Accéléromètres */
    int32_t ax_int = (int32_t)motionData.acc_x;
    int32_t ax_dec = (int32_t)((motionData.acc_x - ax_int) * 10);
    if (ax_dec < 0) ax_dec = -ax_dec;

    int32_t ay_int = (int32_t)motionData.acc_y;
    int32_t ay_dec = (int32_t)((motionData.acc_y - ay_int) * 10);
    if (ay_dec < 0) ay_dec = -ay_dec;

    int32_t az_int = (int32_t)motionData.acc_z;
    int32_t az_dec = (int32_t)((motionData.acc_z - az_int) * 10);
    if (az_dec < 0) az_dec = -az_dec;

    const char* warmup_str = mq2_preheated ? "" : " (warmup)";

    snprintf(buffer, sizeof(buffer),
             "[%lu] T=%d.%dC H=%d.%d%% P=%d.%dhPa "
             "ACC=[%ld.%ld,%ld.%ld,%ld.%ld]mg "
             "Vib=%ld.%ldmg Tilt=%ld.%lddeg GasRaw=%d%s\r\n",
             now,
             t / 10, (t < 0 ? -t : t) % 10,
             h / 10, h % 10,
             p / 10, p % 10,
             ax_int, ax_dec,
             ay_int, ay_dec,
             az_int, az_dec,
             vib_int, vib_dec,
             tilt_int, tilt_dec,
             gasData.adc_raw,
             warmup_str);
    APP_PPRINTF("%s", buffer);
    DisplayAlertes();
}

void DisplayAlertes( void )
{
    char buffer[128];

    if( motionData.alert_vibration_crit ) {
        int32_t vib_int = (int32_t)motionData.vibration;
        int32_t vib_dec = (int32_t)((motionData.vibration - vib_int) * 10);
        if (vib_dec < 0) vib_dec = -vib_dec;
        snprintf(buffer, sizeof(buffer), "  *** CRITICAL VIBRATION: %ld.%ldmg ***\r\n", vib_int, vib_dec);
        APP_PPRINTF("%s", buffer);
    }
    else if( motionData.alert_vibration ) {
        int32_t vib_int = (int32_t)motionData.vibration;
        int32_t vib_dec = (int32_t)((motionData.vibration - vib_int) * 10);
        if (vib_dec < 0) vib_dec = -vib_dec;
        snprintf(buffer, sizeof(buffer), "  [!] Vibration: %ld.%ldmg\r\n", vib_int, vib_dec);
        APP_PPRINTF("%s", buffer);
    }

    if( motionData.alert_tilt_crit ) {
        int32_t tilt_int = (int32_t)motionData.angle_total;
        int32_t tilt_dec = (int32_t)((motionData.angle_total - tilt_int) * 10);
        if (tilt_dec < 0) tilt_dec = -tilt_dec;
        snprintf(buffer, sizeof(buffer), "  *** CRITICAL TILT: %ld.%lddeg ***\r\n", tilt_int, tilt_dec);
        APP_PPRINTF("%s", buffer);
    }
    else if( motionData.alert_tilt ) {
        int32_t tilt_int = (int32_t)motionData.angle_total;
        int32_t tilt_dec = (int32_t)((motionData.angle_total - tilt_int) * 10);
        if (tilt_dec < 0) tilt_dec = -tilt_dec;
        snprintf(buffer, sizeof(buffer), "  [!] Tilt: %ld.%lddeg\r\n", tilt_int, tilt_dec);
        APP_PPRINTF("%s", buffer);
    }

    if( gasData.alert_explosive ) {
#if (MQ2_USE_RAW_THRESHOLD == 1)
        snprintf(buffer, sizeof(buffer), "  *** SOS GAS raw=%d -- EVACUATE! ***\r\n", gasData.adc_raw);
#else
        snprintf(buffer, sizeof(buffer), "  *** SOS GAS: %.0fppm -- EVACUATE! ***\r\n", gasData.ppm);
#endif
        APP_PPRINTF("%s", buffer);
    }
    else if( gasData.alert_critical ) {
#if (MQ2_USE_RAW_THRESHOLD == 1)
        snprintf(buffer, sizeof(buffer), "  *** CRITICAL GAS raw=%d ***\r\n", gasData.adc_raw);
#else
        snprintf(buffer, sizeof(buffer), "  *** CRITICAL GAS: %.0fppm ***\r\n", gasData.ppm);
#endif
        APP_PPRINTF("%s", buffer);
    }
    else if( gasData.alert_gas ) {
#if (MQ2_USE_RAW_THRESHOLD == 1)
        snprintf(buffer, sizeof(buffer), "  [!] Gas raw=%d\r\n", gasData.adc_raw);
#else
        snprintf(buffer, sizeof(buffer), "  [!] Gas: %.0fppm\r\n", gasData.ppm);
#endif
        APP_PPRINTF("%s", buffer);
    }
}

void ProcessAlertes( void )
{
    uint8_t lvl_motion = 0;
    if( motionData.alert_vibration_crit || motionData.alert_tilt_crit )
        lvl_motion = BUZZER_LEVEL_CRIT;
    else if( motionData.alert_vibration || motionData.alert_tilt )
        lvl_motion = BUZZER_LEVEL_WARN;

    uint8_t lvl_gas = 0;
    if( gasData.alert_explosive )      lvl_gas = BUZZER_LEVEL_SOS;
    else if( gasData.alert_critical )  lvl_gas = BUZZER_LEVEL_CRIT;
    else if( gasData.alert_gas )       lvl_gas = BUZZER_LEVEL_WARN;

    if( lvl_motion != last_lvl_motion || lvl_gas != last_lvl_gas ) {
        DisplayAlertes();
    }

    if( lvl_motion != last_lvl_motion ) {
        last_lvl_motion = lvl_motion;
        if( lvl_motion > 0 ) Buzzer_Alert(lvl_motion);
    }
    if( lvl_gas != last_lvl_gas ) {
        last_lvl_gas = lvl_gas;
        if( lvl_gas > 0 ) Buzzer_Alert(lvl_gas);
    }
}

void Sensors_UpdateAll( void )
{
    EnvRead();
    MotionRead();
    GasRead();
    ProcessAlertes();

    /* Gestion manuelle du préchauffage MQ2 (se déclenche après MQ2_PREHEAT_MS ms) */
    if (!mq2_preheated && mq2_initialized && (HAL_GetTick() - mq2_preheat_start_ms >= MQ2_PREHEAT_MS)) {
        mq2_preheated = 1;
        APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Preheat complete (manual)\r\n");
#if (MQ2_USE_RAW_THRESHOLD == 0)
        MQ2_Calibrate();
        APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Ready (ppm mode)\r\n");
#else
        APP_LOG(TS_OFF, VLEVEL_M, "[MQ2] Ready (raw threshold mode)\r\n");
#endif
    }
}

/* USER CODE END EF */