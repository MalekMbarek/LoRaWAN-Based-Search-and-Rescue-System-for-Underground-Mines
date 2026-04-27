/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @author  MCD Application Team
  * @brief   Header for sensors application — PersonalNode (LSM303AGR + MAX30102)
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SENSORS_H__
#define __SENSORS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include "app_config.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/**
  * Sensor data parameters (ST standard interface)
  */
typedef struct
{
  float    pressure;      /*!< in mbar */
  float    temperature;   /*!< in degC */
  float    humidity;      /*!< in % */
  int32_t  latitude;
  int32_t  longitude;
  int16_t  altitudeGps;
  int16_t  altitudeBar;
  /* USER CODE BEGIN sensor_t */
  /* USER CODE END sensor_t */
} sensor_t;

/* USER CODE BEGIN ET */

/**
  * @brief Données accéléromètre LSM303AGR
  */
typedef struct {
  float   acc_x, acc_y, acc_z;      /* mg */
  float   vibration;
  float   angle_x, angle_y, angle_total;
  int16_t raw_x, raw_y, raw_z;      /* mg entiers — pour paquet LoRa */
} AccData_t;

/**
  * @brief Données HR + SpO2 MAX30102
  */
typedef struct {
  uint8_t bpm;
  uint8_t spo2;
  uint8_t bpm_valid;
  uint8_t spo2_valid;
  uint8_t finger_present;
} HrData_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */
extern AccData_t accData;
extern HrData_t  hrData;
extern uint8_t   acc_present;
extern uint8_t   hr_present;
/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  initialize the environmental sensor (ST standard interface)
  */
int32_t EnvSensors_Init(void);

/**
  * @brief  Environmental sensor read (ST standard interface)
  */
int32_t EnvSensors_Read(sensor_t *sensor_data);

/* USER CODE BEGIN EFP */

/**
  * @brief  Init I2C + LSM303AGR + MAX30102
  */
void SensorsInit(void);

/**
  * @brief  Lecture accéléromètre LSM303AGR
  *         À appeler toutes les PN_ACC_READ_PERIOD_MS ms
  */
void AccRead(void);

/**
  * @brief  Lecture FIFO MAX30102 + algo HR/SpO2
  *         À appeler toutes les PN_HR_READ_PERIOD_MS ms
  */
void HrRead(void);

/**
  * @brief  Affichage série (identique ancien Semtech)
  *         ACC X:xxxx.x Y:xxxx.x Z:xxxx.x mg | HR: N bpm | SpO2: N%
  */
void DisplaySensorData(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/