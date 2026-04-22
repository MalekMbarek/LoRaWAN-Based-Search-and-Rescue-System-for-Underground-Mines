/*!
 * \file      app_config.h
 * \brief     Central configuration -- thresholds, pins, intervals
 *
 * PLACE IN:  Core/Inc/app_config.h
 * All other files include this header.
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/* ── I2C sensor addresses ─────────────────────────────────────────────────── */
#ifndef HTS221_I2C_ADDRESS
#define HTS221_I2C_ADDRESS             0x5FU 
#endif
#ifndef LPS22HB_I2C_ADDRESS
#define LPS22HB_I2C_ADDRESS         0xBBU
#endif

/* ── LSM303AGR direct register access ────────────────────────────────────── */
#define ACC_ADDR                    0x32U
#define ACC_REG_WHO_AM_I            0x0FU
#define ACC_REG_CTRL1               0x20U
#define ACC_REG_CTRL4               0x23U
#define ACC_REG_STATUS              0x27U
#define ACC_REG_OUT_X_L             0xA8U   /* bit7=1 → auto-increment */
#define ACC_CTRL1_VAL               0x57U   /* ODR=100Hz, X+Y+Z en      */
#define ACC_CTRL4_VAL               0x88U   /* BDU=1, FS=±8g, HR=1      */
#define ACC_SENSITIVITY_MG          0.98f   /* mg/LSB en HR mode ±8g    */

/* ── Motion alert thresholds ──────────────────────────────────────────────── */
#define THRESHOLD_VIBRATION_MG          200.0f  /* mg  -- niveau 1       */
#define THRESHOLD_VIBRATION_CRITICAL_MG 500.0f  /* mg  -- niveau 2       */
#define THRESHOLD_TILT_DEG               45.0f  /* deg -- niveau 1       */
#define THRESHOLD_TILT_CRITICAL_DEG      70.0f  /* deg -- niveau 2       */

/* ── MQ2 gas sensor (CH4/methane) ─────────────────────────────────────────── */
#define MQ2_ADC_CHANNEL             ADC_CHANNEL_4   /* PA4                */
#define MQ2_RL_KOHM                 20.0f
#define MQ2_RO_CLEAN_AIR_RATIO      9.83f
#define MQ2_ADC_VREF                3.3f
#define MQ2_ADC_MAX                 4095.0f
#define MQ2_VCC                     5.0f
#define MQ2_CH4_A                   4269.6f
#define MQ2_CH4_B                   (-2.648f)
#define MQ2_PREHEAT_MS              30000U          /* 30 s de préchauffage */
#define MQ2_USE_RAW_THRESHOLD       1               /* 1 = seuil brut ADC  */
#define MQ2_RAW_THRESHOLD_WARN      400
#define MQ2_RAW_THRESHOLD_CRIT      800
#define MQ2_RAW_THRESHOLD_EXPL      1500
#define MQ2_THRESHOLD_WARN_PPM      300.0f
#define MQ2_THRESHOLD_CRIT_PPM      1000.0f
#define MQ2_THRESHOLD_EXPLOSIVE_PPM 5000.0f

/* ── Buzzer ───────────────────────────────────────────────────────────────── */
/* PB5 géré dans sys_sensors.c via HAL_GPIO                                  */
#define BUZZER_LEVEL_OK             0
#define BUZZER_LEVEL_WARN           1
#define BUZZER_LEVEL_CRIT           2
#define BUZZER_LEVEL_SOS            3

/* ── Intervalles de temps ─────────────────────────────────────────────────── */
#define DISPLAY_INTERVAL_MS         10000U   /* affichage série toutes 10 s  */
#define SEND_INTERVAL_MS            120000U  /* envoi LoRaWAN toutes 120 s   */
#define JOIN_RETRY_DELAY_MS         10000U   /* délai entre tentatives OTAA  */

/* ── Paquet nœud personnel (raw LoRa) ────────────────────────────────────── */
#define PN_FREQUENCY                868000000U
#define PN_BANDWIDTH                0            /* 0 = 125 kHz            */
#define PN_SF                       7
#define PN_CODINGRATE               1            /* 4/5                    */
#define PN_PREAMBLE                 8
/*
 * Format paquet étendu (12 octets) avec SpO2 :
 *   [0]  type    (0x01)
 *   [1]  node_id
 *   [2]  status  (0=OK 1=FALL 2=SOS)
 *   [3]  bpm
 *   [4]  spo2    (% 0-100)   ← NOUVEAU
 *   [5-6]  acc_x int16 big-endian mg
 *   [7-8]  acc_y
 *   [9-10] acc_z
 *   [11]   réservé
 */
#define PN_PKT_SIZE                 12U          /* taille étendue avec SpO2 */
#define PN_PKT_TYPE                 0x01U
#define PN_MAX_NODES                4

/* ── Macros float → entier (évite -u _printf_float) ──────────────────────── */
#include <math.h>
#define FLOAT_INT(f)   ((int32_t)(f))
#define FLOAT_DEC1(f)  ((int32_t)(fabsf((f) - (float)FLOAT_INT(f)) * 10.0f))

#endif /* __APP_CONFIG_H__ */