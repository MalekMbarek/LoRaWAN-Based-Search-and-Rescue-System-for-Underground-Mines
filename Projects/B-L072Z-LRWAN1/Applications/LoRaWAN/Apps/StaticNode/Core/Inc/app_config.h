/*!
 * \file      app_config.h
 * \brief     Central configuration - thresholds, pins, intervals for Static Node
 * \author    Your Name
 * \date      2026
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/* ============================================================
 * I2C Sensor Addresses
 * ============================================================ */
#ifndef HTS221_I2C_ADDRESS
#define HTS221_I2C_ADDRESS          0x5FU     /* Humidity/Temperature sensor */
#endif
#ifndef LPS22HB_I2C_ADDRESS
#define LPS22HB_I2C_ADDRESS         0xBBU     /* Barometric pressure sensor */
#endif

/* ============================================================
 * LSM303AGR Accelerometer (Direct I2C Register Access)
 * ============================================================ */
#define ACC_ADDR                    0x32U     /* 8-bit write address (AD0=GND) */
#define ACC_REG_WHO_AM_I            0x0FU
#define ACC_REG_CTRL1               0x20U
#define ACC_REG_CTRL4               0x23U
#define ACC_REG_STATUS              0x27U
#define ACC_REG_OUT_X_L             0xA8U     /* Auto-increment enabled (bit7=1) */
#define ACC_CTRL1_VAL               0x57U     /* ODR=100Hz, XYZ enabled */
#define ACC_CTRL4_VAL               0x88U     /* BDU=1, FS=±8g, HR=1 */
#define ACC_SENSITIVITY_MG          0.98f     /* mg/LSB in HR mode ±8g */

/* ============================================================
 * Motion Alert Thresholds
 * ============================================================ */
#define THRESHOLD_VIBRATION_MG          200.0f  /* Warning level (mg) */
#define THRESHOLD_VIBRATION_CRITICAL_MG 500.0f  /* Critical level (mg) */
#define THRESHOLD_TILT_DEG               45.0f  /* Warning level (degrees) */
#define THRESHOLD_TILT_CRITICAL_DEG      70.0f  /* Critical level (degrees) */

/* ============================================================
 * MQ2 Gas Sensor
 * ============================================================
 * Wiring:
 *   MQ2 AOUT -> PA0 (ADC1 channel 0)
 *   MQ2 VCC  -> 3.3V
 *   MQ2 GND  -> GND
 *   MQ2 DOUT -> Not used
 *
 * Note: With 3.3V supply, the heater runs cooler than 5V spec,
 * but the sensor still responds correctly to gas.
 *
 * Rs formula: Rs = ((VCC / Vadc) - 1) * RL
 * where VCC = VREF = 3.3V
 */
#define MQ2_ADC_CHANNEL             ADC_CHANNEL_0   /* PA0 pin */
#define MQ2_GPIO_PIN                GPIO_PIN_0      /* PA0 pin */
#define MQ2_GPIO_PORT               GPIOA
#define MQ2_ADC_VREF                3.3f            /* STM32 VDDA reference (V) */
#define MQ2_ADC_MAX                 4095.0f         /* 12-bit ADC max value */
#define MQ2_VCC                     3.3f            /* MQ2 supply voltage (V) */
#define MQ2_RL_KOHM                 20.0f           /* Load resistor (kOhm) */
#define MQ2_RO_CLEAN_AIR_RATIO      9.83f           /* Rs/Ro ratio in clean air */
#define MQ2_CH4_A                   4269.6f         /* CH4 log-log curve A */
#define MQ2_CH4_B                   -2.648f         /* CH4 log-log curve B */
#define MQ2_PREHEAT_MS              30000U          /* Minimum warmup time (ms) */

/* Detection mode: 1 = raw ADC thresholds, 0 = ppm thresholds */
#define MQ2_USE_RAW_THRESHOLD       0

/* Raw ADC thresholds (0-4095). Tune for your environment. */
#define MQ2_RAW_THRESHOLD_WARN      400
#define MQ2_RAW_THRESHOLD_CRIT      800
#define MQ2_RAW_THRESHOLD_EXPL      1500

/* PPM thresholds (used when MQ2_USE_RAW_THRESHOLD == 0) */
#define MQ2_THRESHOLD_WARN_PPM      300.0f
#define MQ2_THRESHOLD_CRIT_PPM      1000.0f
#define MQ2_THRESHOLD_EXPLOSIVE_PPM 5000.0f

/* ============================================================
 * Buzzer
 * ============================================================
 * Managed in sys_sensors.c via HAL_GPIO. Pin: PB5.
 */
#define BUZZER_LEVEL_OK             0
#define BUZZER_LEVEL_WARN           1
#define BUZZER_LEVEL_CRIT           2
#define BUZZER_LEVEL_SOS            3

#include <math.h>
#define FLOAT_INT(f)   ((int32_t)(f))
#define FLOAT_DEC1(f)  ((int32_t)(fabsf((f) - (float)FLOAT_INT(f)) * 10.0f))

#endif /* __APP_CONFIG_H__ */