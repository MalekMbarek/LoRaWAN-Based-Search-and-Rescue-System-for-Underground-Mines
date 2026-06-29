/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.c — PersonalNode
  * @brief   LSM303AGR (accelerometer/fall detection) + MAX30102 (HR + SpO2)
  *
  * @note    Uses custom I2C handle on PB8/PB9 instead of BSP_I2C1 to avoid
  *          bus conflicts and ensure proper initialization order.
  *          Includes bus recovery with 9 clock pulses before I2C init.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "sys_conf.h"
#include "sys_sensors.h"

/* USER CODE BEGIN Includes */
#include "sys_app.h"
#include "app_config.h"
#include "stm32l0xx_hal.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Custom I2C handle for sensors — independent of ST BSP */
static I2C_HandleTypeDef hpn_i2c;

AccData_t accData;
HrData_t  hrData;
uint8_t   acc_present = 0;
uint8_t   hr_present  = 0;

/* HR/SpO2 buffers */
static float    redBuffer[PN_MAX_SAMPLES];
static float    irBuffer [PN_MAX_SAMPLES];
static uint16_t sampleIndex  = 0;
static uint16_t sampleCount  = 0;
static float    irDc   = 0.0f, redDc  = 0.0f;
static float    maxVal = 0.0f, minVal = 0.0f;
static uint32_t lastBeatTime = 0;
static uint8_t  beatDetected = 0;
static uint32_t bpmBuf[PN_BPM_BUFFER_SIZE];
static uint8_t  bpmBufIdx = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void    I2C_BusRecovery   (void);
static void    PN_I2C_Init       (void);
static void    I2C_Scan          (void);
static void    I2C_WriteReg      (uint8_t devAddr, uint8_t reg, uint8_t val);
static uint8_t I2C_ReadReg       (uint8_t devAddr, uint8_t reg);
static void    I2C_ReadRegs      (uint8_t devAddr, uint8_t reg,
                                   uint8_t *buf, uint8_t len);
static uint8_t ACC_Init_Local    (void);
static uint8_t MAX30102_Init_Local(void);
static void    MAX30102_ReadBoth (uint32_t *red, uint32_t *ir);
/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/
int32_t EnvSensors_Init(void)  { SensorsInit(); return 0; }
int32_t EnvSensors_Read(sensor_t *s)
{
  if (s) { s->temperature = 0.0f; s->humidity = 0.0f; s->pressure = 0.0f; }
  return 0;
}

/* USER CODE BEGIN PrFD */

/**
  * @brief I2C bus recovery by sending 9 clock pulses
  * @note  Called before PN_I2C_Init() to release a slave holding SDA low
  */
static void I2C_BusRecovery(void)
{
  GPIO_InitTypeDef g = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* SCL = PB8 as push-pull output */
  g.Pin   = PN_I2C_SCL_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PN_I2C_SCL_PORT, &g);

  /* SDA = PB9 as input with pull-up */
  g.Pin  = PN_I2C_SDA_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PN_I2C_SDA_PORT, &g);

  /* Generate 9 clock pulses */
  for (int i = 0; i < 9; i++) {
    HAL_GPIO_WritePin(PN_I2C_SCL_PORT, PN_I2C_SCL_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(PN_I2C_SCL_PORT, PN_I2C_SCL_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
    /* Stop if SDA is released */
    if (HAL_GPIO_ReadPin(PN_I2C_SDA_PORT, PN_I2C_SDA_PIN) == GPIO_PIN_SET)
      break;
  }
  /* STOP condition: SCL high then SDA high */
  HAL_GPIO_WritePin(PN_I2C_SCL_PORT, PN_I2C_SCL_PIN, GPIO_PIN_SET);
  HAL_Delay(1);

  /* Reset to analog mode — PN_I2C_Init() will reconfigure as AF */
  g.Pin  = PN_I2C_SCL_PIN | PN_I2C_SDA_PIN;
  g.Mode = GPIO_MODE_ANALOG;
  g.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PN_I2C_SCL_PORT, &g);

  APP_LOG(TS_OFF, VLEVEL_M, "I2C bus recovery done\r\n");
}

/**
  * @brief Initialize custom I2C1 on PB8/PB9
  * @note  Independent of ST BSP. Same configuration as the legacy Semtech version.
  */
static void PN_I2C_Init(void)
{
  /* Enable clocks */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();

  /* PB8 = SCL, PB9 = SDA — open-drain with internal pull-up */
  GPIO_InitTypeDef g = {0};
  g.Pin       = PN_I2C_SCL_PIN | PN_I2C_SDA_PIN;
  g.Mode      = GPIO_MODE_AF_OD;
  g.Pull      = GPIO_PULLUP;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = PN_I2C_AF;
  HAL_GPIO_Init(PN_I2C_SCL_PORT, &g);

  /* Reset I2C peripheral before init */
  __HAL_RCC_I2C1_FORCE_RESET();
  HAL_Delay(5);
  __HAL_RCC_I2C1_RELEASE_RESET();

  /* HAL configuration */
  hpn_i2c.Instance             = PN_I2C_INSTANCE;
  hpn_i2c.Init.Timing          = PN_I2C_TIMING;
  hpn_i2c.Init.OwnAddress1     = 0;
  hpn_i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hpn_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hpn_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hpn_i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

  HAL_StatusTypeDef ret = HAL_I2C_Init(&hpn_i2c);
  APP_LOG(TS_OFF, VLEVEL_M, "PN_I2C_Init: %s\r\n",
          ret == HAL_OK ? "OK" : "FAIL");
}

/**
  * @brief Scan I2C bus for devices
  * @note  5ms timeout per address, total ~640ms max
  */
static void I2C_Scan(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "I2C scan (7-bit addr, 5ms timeout):\r\n");
  uint8_t n = 0;
  for (uint8_t addr7 = 1; addr7 < 128; addr7++) {
    if (HAL_I2C_IsDeviceReady(&hpn_i2c,
                               (uint16_t)(addr7 << 1),
                               1, 5) == HAL_OK) {
      APP_LOG(TS_OFF, VLEVEL_M,
              "  Found 7-bit=0x%02X (8-bit W=0x%02X)\r\n",
              (unsigned)addr7, (unsigned)(addr7 << 1));
      n++;
    }
  }
  if (n == 0)
    APP_LOG(TS_OFF, VLEVEL_M,
            "  [WARN] No devices found -- bus dead or wrong pins\r\n");
  APP_LOG(TS_OFF, VLEVEL_M, "Scan complete: %d device(s)\r\n", (int)n);
}

/* I2C Helpers */

static void I2C_WriteReg(uint8_t devAddr, uint8_t reg, uint8_t val)
{
  uint8_t buf[2] = { reg, val };
  HAL_I2C_Master_Transmit(&hpn_i2c, devAddr, buf, 2,
                           PN_I2C_TIMEOUT_MS);
}

static uint8_t I2C_ReadReg(uint8_t devAddr, uint8_t reg)
{
  uint8_t val = 0;
  HAL_I2C_Master_Transmit(&hpn_i2c, devAddr, &reg, 1,
                           PN_I2C_TIMEOUT_MS);
  HAL_I2C_Master_Receive (&hpn_i2c, devAddr | 0x01U, &val, 1,
                           PN_I2C_TIMEOUT_MS);
  return val;
}

static void I2C_ReadRegs(uint8_t devAddr, uint8_t reg,
                          uint8_t *buf, uint8_t len)
{
  HAL_I2C_Master_Transmit(&hpn_i2c, devAddr, &reg, 1,
                           PN_I2C_TIMEOUT_MS);
  HAL_I2C_Master_Receive (&hpn_i2c, devAddr | 0x01U, buf, len,
                           PN_I2C_TIMEOUT_MS);
}

/**
  * @brief Initialize LSM303AGR accelerometer
  * @return 1 if successful, 0 if not found
  */
static uint8_t ACC_Init_Local(void)
{
  /* Test primary address 0x32 (SDO=GND) */
  uint8_t who = I2C_ReadReg(ACC_ADDR_WRITE, ACC_REG_WHO_AM_I);
  APP_LOG(TS_OFF, VLEVEL_M,
          "LSM303AGR WHO_AM_I @0x%02X = 0x%02X (expect 0x33)\r\n",
          (unsigned)ACC_ADDR_WRITE, (unsigned)who);

  if (who != 0x33) {
    /* Try alternative address 0x3A (SDO=VDD) */
    who = I2C_ReadReg(0x3AU, ACC_REG_WHO_AM_I);
    APP_LOG(TS_OFF, VLEVEL_M,
            "LSM303AGR WHO_AM_I @0x3A = 0x%02X (expect 0x33)\r\n",
            (unsigned)who);
  }

  if (who == 0x33) {
    I2C_WriteReg(ACC_ADDR_WRITE, ACC_CTRL_REG1, ACC_CTRL_REG1_VAL);
    I2C_WriteReg(ACC_ADDR_WRITE, ACC_CTRL_REG4, ACC_CTRL_REG4_VAL);
    APP_LOG(TS_OFF, VLEVEL_M, "LSM303AGR ready\r\n");
    return 1;
  }

  APP_LOG(TS_OFF, VLEVEL_M, "[WARN] LSM303AGR not found -- ACC disabled\r\n");
  return 0;
}

/**
  * @brief Read LSM303AGR accelerometer data
  * @note  Call every PN_ACC_READ_PERIOD_MS ms
  */
void AccRead(void)
{
  if (!acc_present) return;

  uint8_t status = I2C_ReadReg(ACC_ADDR_WRITE, ACC_STATUS_REG);
  if (!(status & 0x08U)) return;   /* ZYXDA not ready */

  uint8_t raw[6] = {0};
  I2C_ReadRegs(ACC_ADDR_WRITE, ACC_OUT_X_L, raw, 6);

  int16_t rx = (int16_t)((raw[1] << 8) | raw[0]) >> 4;
  int16_t ry = (int16_t)((raw[3] << 8) | raw[2]) >> 4;
  int16_t rz = (int16_t)((raw[5] << 8) | raw[4]) >> 4;

  accData.acc_x = (float)rx * ACC_SENSITIVITY_MG;
  accData.acc_y = (float)ry * ACC_SENSITIVITY_MG;
  accData.acc_z = (float)rz * ACC_SENSITIVITY_MG;
  accData.raw_x = (int16_t)accData.acc_x;
  accData.raw_y = (int16_t)accData.acc_y;
  accData.raw_z = (int16_t)accData.acc_z;

  float norm = sqrtf(accData.acc_x * accData.acc_x +
                     accData.acc_y * accData.acc_y +
                     accData.acc_z * accData.acc_z);
  accData.vibration   = fabsf(norm - 1000.0f);
  accData.angle_x     = atan2f(accData.acc_x,
      sqrtf(accData.acc_y * accData.acc_y + accData.acc_z * accData.acc_z))
      * 180.0f / (float)M_PI;
  accData.angle_y     = atan2f(accData.acc_y,
      sqrtf(accData.acc_x * accData.acc_x + accData.acc_z * accData.acc_z))
      * 180.0f / (float)M_PI;
  accData.angle_total = sqrtf(accData.angle_x * accData.angle_x +
                              accData.angle_y * accData.angle_y);
}

/**
  * @brief Read both RED and IR channels from MAX30102 FIFO
  */
static void MAX30102_ReadBoth(uint32_t *red, uint32_t *ir)
{
  uint8_t raw[6] = {0};
  I2C_ReadRegs(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_DATA, raw, 6);
  *red = ((uint32_t)(raw[0] & 0x03U) << 16) |
         ((uint32_t)raw[1] << 8) | (uint32_t)raw[2];
  *ir  = ((uint32_t)(raw[3] & 0x03U) << 16) |
         ((uint32_t)raw[4] << 8) | (uint32_t)raw[5];
}

/**
  * @brief Initialize MAX30102 sensor
  * @return 1 if successful, 0 if not found
  */
static uint8_t MAX30102_Init_Local(void)
{
  /* Software reset */
  I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_MODE_CONFIG, 0x40U);
  HAL_Delay(100);

  uint8_t partId = I2C_ReadReg(MAX30102_ADDR_WRITE, MAX30102_REG_PART_ID);
  APP_LOG(TS_OFF, VLEVEL_M,
          "MAX30102 PART_ID @0x%02X = 0x%02X (expect 0x15)\r\n",
          (unsigned)MAX30102_ADDR_WRITE, (unsigned)partId);

  if (partId == 0x15U) {
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_MODE_CONFIG,  0x03U);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_CONFIG,  0x10U);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_SPO2_CONFIG,  0x6BU);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_LED1_PA,      0xFFU);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_LED2_PA,      0xFFU);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_WR_PTR,  0x00U);
    I2C_WriteReg(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_RD_PTR,  0x00U);
    memset(bpmBuf, 0, sizeof(bpmBuf));
    APP_LOG(TS_OFF, VLEVEL_M, "MAX30102 ready\r\n");
    return 1;
  }

  APP_LOG(TS_OFF, VLEVEL_M,
          "[WARN] MAX30102 not found -- HR/SpO2 disabled\r\n");
  return 0;
}

/**
  * @brief Read MAX30102 FIFO and process HR/SpO2 algorithm
  * @note  Call every PN_HR_READ_PERIOD_MS ms
  */
void HrRead(void)
{
  if (!hr_present) return;

  uint8_t wrPtr = I2C_ReadReg(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_WR_PTR);
  uint8_t rdPtr = I2C_ReadReg(MAX30102_ADDR_WRITE, MAX30102_REG_FIFO_RD_PTR);
  uint8_t n = (wrPtr >= rdPtr) ? (wrPtr - rdPtr) : (32U - rdPtr + wrPtr);

  for (uint8_t i = 0; i < n; i++) {
    uint32_t red, ir;
    MAX30102_ReadBoth(&red, &ir);
    float fRed = (float)red;
    float fIr  = (float)ir;

    /* No finger detected */
    if (fIr < 50000.0f) {
      sampleCount = 0;
      hrData.bpm_valid = 0; hrData.spo2_valid = 0;
      hrData.finger_present = 0;
      beatDetected = 0; lastBeatTime = 0;
      irDc = 0.0f; redDc = 0.0f; maxVal = 0.0f; minVal = 0.0f;
      continue;
    }
    hrData.finger_present = 1;

    /* DC removal */
    if (sampleCount == 0) { irDc = fIr; redDc = fRed; }
    else {
      irDc  = irDc  * (1.0f - PN_DC_FILTER_ALPHA) + fIr  * PN_DC_FILTER_ALPHA;
      redDc = redDc * (1.0f - PN_DC_FILTER_ALPHA) + fRed * PN_DC_FILTER_ALPHA;
    }
    float irAc  = fIr  - irDc;
    float redAc = fRed - redDc;

    redBuffer[sampleIndex] = redAc;
    irBuffer [sampleIndex] = irAc;
    sampleIndex = (sampleIndex + 1U) % PN_MAX_SAMPLES;
    if (sampleCount < PN_MAX_SAMPLES) sampleCount++;

    /* Peak detection for BPM */
    if (sampleCount > 10U) {
      if (irAc > maxVal) maxVal = irAc;
      if (irAc < minVal) minVal = irAc;
      float thr = (maxVal - minVal) * PN_PEAK_THRESHOLD;

      if (!beatDetected && irAc > thr) {
        beatDetected = 1;
        uint32_t now = HAL_GetTick();
        if (lastBeatTime != 0U) {
          uint32_t interval = now - lastBeatTime;
          if (interval > (60000U / (uint32_t)PN_MAX_BPM) &&
              interval < (60000U / (uint32_t)PN_MIN_BPM)) {
            bpmBuf[bpmBufIdx % PN_BPM_BUFFER_SIZE] = 60000U / interval;
            bpmBufIdx++;
            if (bpmBufIdx >= PN_BPM_BUFFER_SIZE) {
              uint32_t sum = 0;
              for (uint8_t j = 0; j < PN_BPM_BUFFER_SIZE; j++) sum += bpmBuf[j];
              hrData.bpm = (uint8_t)(sum / PN_BPM_BUFFER_SIZE);
              hrData.bpm_valid = 1;
            }
          }
        }
        lastBeatTime = now;
      } else if (beatDetected && irAc < -thr) {
        beatDetected = 0;
      }
      /* Reset peak tracking every 5 seconds */
      if ((HAL_GetTick() % 5000U) < 10U) { maxVal = 0.0f; minVal = 0.0f; }
    }
  }

  /* SpO2 calculation */
  if (sampleCount >= (uint16_t)PN_SPO2_MIN_SAMPLES && hrData.bpm_valid) {
    uint16_t start = (sampleIndex + PN_MAX_SAMPLES - PN_SPO2_MIN_SAMPLES)
                     % PN_MAX_SAMPLES;
    float sumR = 0.0f, sumI = 0.0f;
    for (uint8_t i = 0; i < PN_SPO2_MIN_SAMPLES; i++) {
      uint16_t idx = (start + i) % PN_MAX_SAMPLES;
      sumR += fabsf(redBuffer[idx]);
      sumI += fabsf(irBuffer [idx]);
    }
    if (sumI > 0.0f) {
      int spo2 = (int)(110.0f - 25.0f * (sumR / sumI));
      if (spo2 < 70)  spo2 = 70;
      if (spo2 > 100) spo2 = 100;
      hrData.spo2 = (uint8_t)spo2;
      hrData.spo2_valid = 1;
    }
  }
}

/**
  * @brief Initialize all sensors
  * @note  Order: bus recovery → I2C init → scan → ACC init → HR init
  */
void SensorsInit(void)
{
  APP_LOG(TS_OFF, VLEVEL_M, "\r\n-- SensorsInit --\r\n");

  /* 1. Release bus if locked */
  I2C_BusRecovery();

  /* 2. Initialize custom I2C1 on PB8/PB9 */
  PN_I2C_Init();

  /* 3. Diagnostic scan */
  I2C_Scan();

  /* 4. Initialize sensors */
  acc_present = ACC_Init_Local();
  hr_present  = MAX30102_Init_Local();

  memset(&accData, 0, sizeof(accData));
  memset(&hrData,  0, sizeof(hrData));

  APP_LOG(TS_OFF, VLEVEL_M,
          "SensorsInit done: ACC=%d HR=%d\r\n\r\n",
          (int)acc_present, (int)hr_present);
}

/**
  * @brief Display sensor data on serial output
  * @note  Format: ACC X:xxxx.x Y:xxxx.x Z:xxxx.x mg | HR: N bpm | SpO2: N%
  */
void DisplaySensorData(void)
{
  char line[160];
  int16_t xi = accData.raw_x, xd = (int16_t)((xi < 0 ? -xi : xi) % 10);
  int16_t yi = accData.raw_y, yd = (int16_t)((yi < 0 ? -yi : yi) % 10);
  int16_t zi = accData.raw_z, zd = (int16_t)((zi < 0 ? -zi : zi) % 10);

  if (acc_present) {
    if (hr_present) {
      if      (hrData.bpm_valid && hrData.spo2_valid)
        snprintf(line, sizeof(line),
                 "ACC X:%4d.%d Y:%4d.%d Z:%4d.%d mg | HR: %d bpm | SpO2: %d%%\r\n",
                 xi, xd, yi, yd, zi, zd, (int)hrData.bpm, (int)hrData.spo2);
      else if (hrData.bpm_valid)
        snprintf(line, sizeof(line),
                 "ACC X:%4d.%d Y:%4d.%d Z:%4d.%d mg | HR: %d bpm | SpO2: --\r\n",
                 xi, xd, yi, yd, zi, zd, (int)hrData.bpm);
      else if (hrData.spo2_valid)
        snprintf(line, sizeof(line),
                 "ACC X:%4d.%d Y:%4d.%d Z:%4d.%d mg | HR: -- | SpO2: %d%%\r\n",
                 xi, xd, yi, yd, zi, zd, (int)hrData.spo2);
      else
        snprintf(line, sizeof(line),
                 "ACC X:%4d.%d Y:%4d.%d Z:%4d.%d mg | HR: -- | SpO2: --\r\n",
                 xi, xd, yi, yd, zi, zd);
    } else {
      snprintf(line, sizeof(line),
               "ACC X:%4d.%d Y:%4d.%d Z:%4d.%d mg | HR: disabled\r\n",
               xi, xd, yi, yd, zi, zd);
    }
  } else {
    snprintf(line, sizeof(line), "ACC: disabled | HR: %s\r\n",
             !hr_present      ? "disabled" :
             hrData.bpm_valid ? "OK" : "--");
  }
  APP_LOG(TS_OFF, VLEVEL_M, "%s", line);
}

/* USER CODE END PrFD */