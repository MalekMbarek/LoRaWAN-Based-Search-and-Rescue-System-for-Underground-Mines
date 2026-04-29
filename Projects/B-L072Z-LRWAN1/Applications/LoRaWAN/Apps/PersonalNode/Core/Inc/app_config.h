/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   PersonalNode — Configuration centrale
  *
  * ── STRATÉGIE I2C ────────────────────────────────────────────────────────────
  *
  * La BSP ST (BSP_I2C1_xxx) est IGNORÉE pour les capteurs personnels.
  * sys_sensors.c gère son propre handle I2C (hpn_i2c) initialisé sur
  * PB8 (SCL) / PB9 (SDA), exactement comme la version Semtech qui fonctionnait.
  *
  * Raisons :
  *   1. BSP_I2C1 peut utiliser un bus différent selon la BSP
  *   2. hbus_i2c1 est static dans b_l072z_lrwan1_bus.c → inaccessible
  *   3. Gpio_PreInit() dans sys_app.c met PB8/PB9 en analogique avant
  *      que BSP_I2C1_Init() les reconfigure → bus bloqué au démarrage
  *
  * Solution : init I2C propre dans SensorsInit() après Gpio_PreInit(),
  * avec récupération de bus (9 CLK bit-bang) si nécessaire.
  *
  * Adresses : 8 bits WRITE (HAL_I2C_Master_Transmit attend addr<<1 déjà fait)
  *   LSM303AGR : 0x32 write / 0x33 read (SDO=GND)
  *   MAX30102  : 0xAE write / 0xAF read (fixe)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── Identifiant du nœud ─────────────────────────────────────────────────── */
#ifndef MY_NODE_ID
#define MY_NODE_ID              1U
#endif

/* ── Format paquet LoRa 11 bytes ─────────────────────────────────────────── */
#define PKT_TYPE_FULL           0x01U
#define PKT_SIZE                11U
#define PKT_I_TYPE              0U
#define PKT_I_ID                1U
#define PKT_I_STATUS            2U
#define PKT_I_BPM               3U
#define PKT_I_AX_H              4U
#define PKT_I_AX_L              5U
#define PKT_I_AY_H              6U
#define PKT_I_AY_L              7U
#define PKT_I_AZ_H              8U
#define PKT_I_AZ_L              9U
#define PKT_I_SPO2              10U

#define STATUS_OK               0U
#define STATUS_FALL             1U
#define STATUS_SOS              2U

/* ── LoRa ────────────────────────────────────────────────────────────────── */
#define PN_RF_FREQUENCY         868000000UL
#define PN_TX_OUTPUT_POWER      14
#define PN_BANDWIDTH            0
#define PN_SPREADING_FACTOR     7
#define PN_CODINGRATE           1
#define PN_PREAMBLE_LENGTH      8
#define PN_TX_TIMEOUT_MS        3000U

/* ── Timing ──────────────────────────────────────────────────────────────── */
#define PN_ACC_READ_PERIOD_MS   50U
#define PN_HR_READ_PERIOD_MS    10U
#define PN_TX_PERIOD_MS         10000U
#define PN_PRINT_PERIOD_MS      3000U

/* ── Détection de chute ──────────────────────────────────────────────────── */
#define PN_FREE_FALL_THRESHOLD_MG   700.0f
#define PN_IMPACT_THRESHOLD_MG      1300.0f
#define PN_FALL_WINDOW_MS           800U

/* ── I2C hardware dédié capteurs ─────────────────────────────────────────────
 * Identique version Semtech : I2C1, PB8=SCL, PB9=SDA, open-drain + pull-up
 * Timing 0x00707CBB → 100 kHz @ 32 MHz HSI (STM32L072)
 * ────────────────────────────────────────────────────────────────────────── */
#define PN_I2C_INSTANCE         I2C1
#define PN_I2C_SCL_PORT         GPIOB
#define PN_I2C_SCL_PIN          GPIO_PIN_8
#define PN_I2C_SDA_PORT         GPIOB
#define PN_I2C_SDA_PIN          GPIO_PIN_9
#define PN_I2C_AF               GPIO_AF4_I2C1
#define PN_I2C_TIMING           0x00707CBBU
#define PN_I2C_TIMEOUT_MS       10U

/* ── LSM303AGR — adresses 8 bits (SDO/SA1 = GND) ───────────────────────── */
#define ACC_ADDR_WRITE          0x32U
#define ACC_REG_WHO_AM_I        0x0FU
#define ACC_CTRL_REG1           0x20U
#define ACC_CTRL_REG4           0x23U
#define ACC_STATUS_REG          0x27U
#define ACC_OUT_X_L             0xA8U
#define ACC_CTRL_REG1_VAL       0x57U
#define ACC_CTRL_REG4_VAL       0x88U
#define ACC_SENSITIVITY_MG      0.98f

/* ── MAX30102 — adresses 8 bits (fixe) ──────────────────────────────────── */
#define MAX30102_ADDR_WRITE         0xAEU
#define MAX30102_REG_FIFO_WR_PTR    0x04U
#define MAX30102_REG_FIFO_RD_PTR    0x06U
#define MAX30102_REG_FIFO_DATA      0x07U
#define MAX30102_REG_FIFO_CONFIG    0x08U
#define MAX30102_REG_MODE_CONFIG    0x09U
#define MAX30102_REG_SPO2_CONFIG    0x0AU
#define MAX30102_REG_LED1_PA        0x0CU
#define MAX30102_REG_LED2_PA        0x0DU
#define MAX30102_REG_PART_ID        0xFFU

/* ── Algorithme HR/SpO2 ──────────────────────────────────────────────────── */
#define PN_MAX_SAMPLES          100U
#define PN_DC_FILTER_ALPHA      0.01f
#define PN_PEAK_THRESHOLD       0.1f
#define PN_MIN_BPM              40U
#define PN_MAX_BPM              200U
#define PN_BPM_BUFFER_SIZE      5U
#define PN_SPO2_MIN_SAMPLES     50U

/* ── GPIO ────────────────────────────────────────────────────────────────── */
#define SOS_BUTTON_PORT         GPIOA
#define SOS_BUTTON_PIN          GPIO_PIN_0
#define SOS_BUTTON_IRQn         EXTI0_1_IRQn
#define LED_SOS_PORT            GPIOA
#define LED_SOS_PIN             GPIO_PIN_5
#define LED_HEARTBEAT_PORT      GPIOB
#define LED_HEARTBEAT_PIN       GPIO_PIN_13

#ifdef __cplusplus
}
#endif

#endif /* __APP_CONFIG_H__ */