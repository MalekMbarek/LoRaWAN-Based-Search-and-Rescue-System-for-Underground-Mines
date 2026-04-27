/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @brief   Mine monitoring sensors + buzzer + alerts
  * PLACE IN: Core/Inc/sys_sensors.h
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  Structures de données
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Structure d'origine conservée pour compatibilité avec la pile ST
 *        (utilisée par EnvSensors_Read appelé dans lora_app.c)
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
 * @brief Données environnementales stockées en entiers ×10
 *        pour éviter printf float (nano.specs)
 *        ex: temperature=247  → 24.7 °C
 *            humidity=1000    → 100.0 %
 *            pressure=10242   → 1024.2 hPa
 */
typedef struct {
  int16_t  temperature;   /* °C  × 10 */
  uint16_t humidity;      /* %   × 10 */
  uint16_t pressure;      /* hPa × 10 */
} EnvData_t;

/**
 * @brief Données mouvement issues du LSM303AGR (float, mg + degrés)
 */
typedef struct {
  float   acc_x;
  float   acc_y;
  float   acc_z;
  float   vibration;           /* écart à la norme 1g au repos (mg) */
  float   angle_x;             /* degrés                            */
  float   angle_y;             /* degrés                            */
  float   angle_total;         /* norme inclinaison (degrés)        */
  uint8_t alert_vibration;
  uint8_t alert_vibration_crit;
  uint8_t alert_tilt;
  uint8_t alert_tilt_crit;
} MotionData_t;

/**
 * @brief Données gaz MQ2
 */
typedef struct {
  uint16_t adc_raw;
  float    rs_ro_ratio;
  float    ppm;
  uint8_t  alert_gas;
  uint8_t  alert_critical;
  uint8_t  alert_explosive;
} GasData_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Variables globales exportées
 * ═══════════════════════════════════════════════════════════════════════════ */
extern EnvData_t    envData;
extern MotionData_t motionData;
extern GasData_t    gasData;
extern uint8_t      mq2_initialized;
extern uint8_t      mq2_preheated;

/* ── Macros IKS01A2 (si SENSOR_ENABLED) ─────────────────────────────────── */
#if defined(SENSOR_ENABLED) && (SENSOR_ENABLED == 1) && defined(X_NUCLEO_IKS01A2)
#define HTS221_0   0U
#define LPS22HB_0  1U
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  Prototypes
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Requis par la pile ST LoRaWAN */
int32_t EnvSensors_Init(void);
int32_t EnvSensors_Read(sensor_t *sensor_data);

/* Initialisation globale */
void SensorsInit       (void);

/* Lectures individuelles */
void EnvRead           (void);
void MotionRead        (void);
void GasRead           (void);
void MQ2_Calibrate     (void);

/* Affichage */
void DisplaySensorData (void);
void DisplayAlertes    (void);

/* Alertes + buzzer */
void ProcessAlertes    (void);
void Sensors_UpdateAll (void);
void Buzzer_On         (void);
void Buzzer_Off        (void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/