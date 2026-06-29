/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @brief   Mine monitoring sensors - LSM303AGR, HTS221, LPS22HB, MQ2 gas sensor
  *          with buzzer and alert management
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SENSORS_H__
#define __SENSORS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Data Structures
 * ============================================================ */

/**
 * @brief Legacy ST stack compatibility structure
 * @note  Used by EnvSensors_Read() called from lora_app.c
 */
typedef struct
{
  float    pressure;
  float    temperature;
  float    humidity;
  int32_t  latitude;
  int32_t  longitude;
  int16_t  altitudeGps;
  int16_t  altitudeBar;
} sensor_t;

/**
 * @brief Environmental data stored as integers ×10
 * @note  Avoids printf float dependency (nano.specs)
 *        Example: temperature=247  → 24.7 °C
 *                 humidity=1000    → 100.0 %
 *                 pressure=10242   → 1024.2 hPa
 */
typedef struct {
  int16_t  temperature;   /* °C  × 10 */
  uint16_t humidity;      /* %   × 10 */
  uint16_t pressure;      /* hPa × 10 */
} EnvData_t;

/**
 * @brief Motion data from LSM303AGR (float, mg + degrees)
 */
typedef struct {
  float   acc_x;
  float   acc_y;
  float   acc_z;
  float   vibration;           /* Deviation from 1g at rest (mg) */
  float   angle_x;             /* Degrees */
  float   angle_y;             /* Degrees */
  float   angle_total;         /* Tilt magnitude (degrees) */
  uint8_t alert_vibration;
  uint8_t alert_vibration_crit;
  uint8_t alert_tilt;
  uint8_t alert_tilt_crit;
} MotionData_t;

/**
 * @brief MQ2 gas sensor data
 */
typedef struct {
  uint16_t adc_raw;
  float    rs_ro_ratio;
  float    ppm;
  uint8_t  alert_gas;
  uint8_t  alert_critical;
  uint8_t  alert_explosive;
} GasData_t;

/* ============================================================
 * Exported Global Variables
 * ============================================================ */
extern EnvData_t    envData;
extern MotionData_t motionData;
extern GasData_t    gasData;
extern uint8_t      mq2_initialized;
extern uint8_t      mq2_preheated;

/* IKS01A2 Macros (if SENSOR_ENABLED) */
#if defined(SENSOR_ENABLED) && (SENSOR_ENABLED == 1) && defined(X_NUCLEO_IKS01A2)
#define HTS221_0   0U
#define LPS22HB_0  1U
#endif

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/* ST LoRaWAN stack required functions */
int32_t EnvSensors_Init(void);
int32_t EnvSensors_Read(sensor_t *sensor_data);

/* Global initialization */
void SensorsInit       (void);

/* Individual sensor reads */
void EnvRead           (void);
void MotionRead        (void);
void GasRead           (void);
void MQ2_Calibrate     (void);

/* Display functions */
void DisplaySensorData (void);
void DisplayAlertes    (void);

/* Alert and buzzer control */
void ProcessAlertes    (void);
void Sensors_UpdateAll (void);
void Buzzer_On         (void);
void Buzzer_Off        (void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/