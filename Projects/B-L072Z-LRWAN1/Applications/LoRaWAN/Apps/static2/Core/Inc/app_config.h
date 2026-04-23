/*!
 * \file      app_config.h
 * \brief     Central configuration -- thresholds, pins, intervals
 * PLACE IN:  Core/Inc/app_config.h
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/* ── I2C sensor addresses ─────────────────────────────────────────────────── */
#ifndef HTS221_I2C_ADDRESS
#define HTS221_I2C_ADDRESS          0x5FU   /* SDO pulled high on IKS01A2    */
#endif
#ifndef LPS22HB_I2C_ADDRESS
#define LPS22HB_I2C_ADDRESS         0xBBU
#endif

/* ── LSM303AGR accelerometer (direct I2C register access) ────────────────── */
#define ACC_ADDR                    0x32U   /* 8-bit write addr (AD0=GND)    */
#define ACC_REG_WHO_AM_I            0x0FU
#define ACC_REG_CTRL1               0x20U
#define ACC_REG_CTRL4               0x23U
#define ACC_REG_STATUS              0x27U
#define ACC_REG_OUT_X_L             0xA8U   /* bit7=1 enables auto-increment */
#define ACC_CTRL1_VAL               0x57U   /* ODR=100Hz, XYZ enabled        */
#define ACC_CTRL4_VAL               0x88U   /* BDU=1, FS=±8g, HR=1           */
#define ACC_SENSITIVITY_MG          0.98f   /* mg/LSB in HR mode ±8g         */

/* ── Motion alert thresholds ──────────────────────────────────────────────── */
#define THRESHOLD_VIBRATION_MG          200.0f  /* mg  -- warning            */
#define THRESHOLD_VIBRATION_CRITICAL_MG 500.0f  /* mg  -- critical           */
#define THRESHOLD_TILT_DEG               45.0f  /* deg -- warning            */
#define THRESHOLD_TILT_CRITICAL_DEG      70.0f  /* deg -- critical           */

/* ── MQ2 gas sensor ───────────────────────────────────────────────────────── */
/*
 * Physical wiring:
 *   MQ2 AOUT -> PA0  (ADC1 channel 0)
 *   MQ2 VCC  -> 3.3V
 *   MQ2 GND  -> GND
 *   MQ2 DOUT -> not used
 *
 * At 3.3V supply the heater runs cooler than the datasheet 5V spec,
 * but the sensor still responds correctly to gas.
 * Rs formula: Rs = ((VCC / Vadc) - 1) * RL   with VCC = VREF = 3.3V
 */
#define MQ2_ADC_CHANNEL             ADC_CHANNEL_0   /* PA0                   */
#define MQ2_GPIO_PIN                GPIO_PIN_0       /* PA0                   */
#define MQ2_GPIO_PORT               GPIOA
#define MQ2_ADC_VREF                3.3f             /* STM32 VDDA reference  */
#define MQ2_ADC_MAX                 4095.0f          /* 12-bit ADC            */
#define MQ2_VCC                     3.3f             /* MQ2 supply voltage    */
#define MQ2_RL_KOHM                 20.0f            /* load resistor kOhm    */
#define MQ2_RO_CLEAN_AIR_RATIO      9.83f            /* Rs/Ro in clean air    */
#define MQ2_CH4_A                   4269.6f          /* CH4 log-log curve A   */
#define MQ2_CH4_B                   (-2.648f)        /* CH4 log-log curve B   */
#define MQ2_PREHEAT_MS              30000U           /* 30 s minimum warmup   */

/* Detection mode: 1 = raw ADC thresholds (simpler), 0 = ppm thresholds     */
#define MQ2_USE_RAW_THRESHOLD       1

/* Raw ADC thresholds (0-4095). Tune these for your environment.             */
#define MQ2_RAW_THRESHOLD_WARN      400
#define MQ2_RAW_THRESHOLD_CRIT      800
#define MQ2_RAW_THRESHOLD_EXPL      1500

/* PPM thresholds (only used when MQ2_USE_RAW_THRESHOLD == 0)               */
#define MQ2_THRESHOLD_WARN_PPM      300.0f
#define MQ2_THRESHOLD_CRIT_PPM      1000.0f
#define MQ2_THRESHOLD_EXPLOSIVE_PPM 5000.0f

/* ── Buzzer ───────────────────────────────────────────────────────────────── */
/* Managed in sys_sensors.c via HAL_GPIO. Pin: PB5.                         */
#define BUZZER_LEVEL_OK             0
#define BUZZER_LEVEL_WARN           1
#define BUZZER_LEVEL_CRIT           2
#define BUZZER_LEVEL_SOS            3

/* ── Time intervals ───────────────────────────────────────────────────────── */
#define DISPLAY_INTERVAL_MS         60000U  /* serial log every 60 s         */
#define SEND_INTERVAL_MS            120000U /* LoRaWAN uplink every 120 s    */
#define JOIN_RETRY_DELAY_MS         10000U  /* delay between OTAA retries    */

/* ── Raw LoRa personal node packet ───────────────────────────────────────── */
#define PN_FREQUENCY                868000000U
#define PN_BANDWIDTH                0           /* 0 = 125 kHz               */
#define PN_SF                       7
#define PN_CODINGRATE               1           /* 4/5                       */
#define PN_PREAMBLE                 8
/*
 * Packet format (12 bytes) including SpO2:
 *   [0]    packet type  (0x01)
 *   [1]    node_id
 *   [2]    status       (0=OK, 1=FALL, 2=SOS)
 *   [3]    bpm          (heart rate beats/min)
 *   [4]    spo2         (SpO2 %, 0-100)
 *   [5-6]  acc_x        int16 big-endian, mg
 *   [7-8]  acc_y
 *   [9-10] acc_z
 *   [11]   reserved
 */
#define PN_PKT_SIZE                 12U
#define PN_PKT_TYPE                 0x01U
#define PN_MAX_NODES                4

/* ── Float-to-integer display macros (avoids -u _printf_float linker flag) ─ */
#include <math.h>
#define FLOAT_INT(f)   ((int32_t)(f))
#define FLOAT_DEC1(f)  ((int32_t)(fabsf((f) - (float)FLOAT_INT(f)) * 10.0f))

#endif /* __APP_CONFIG_H__ */