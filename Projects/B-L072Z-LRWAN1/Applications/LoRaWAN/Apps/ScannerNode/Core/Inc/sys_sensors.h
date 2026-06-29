/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sys_sensors.h
  * @author  MCD Application Team
  * @brief   Header for sensor application - ScannerNode
  *          - Environmental sensors (HTS221, LPS22HB)
  *          - ST standard sensor interface
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

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/

/**
  * @brief Sensor data parameters (ST standard interface)
  */
typedef struct
{
  float pressure;         /*!< Pressure in mbar */
  float temperature;      /*!< Temperature in degC */
  float humidity;         /*!< Humidity in % */
  int32_t latitude;       /*!< Latitude (converted to binary) */
  int32_t longitude;      /*!< Longitude (converted to binary) */
  int16_t altitudeGps;    /*!< GPS altitude in meters */
  int16_t altitudeBar;    /*!< Barometric altitude in m * 10 */
  /* USER CODE BEGIN sensor_t */

  /* USER CODE END sensor_t */
} sensor_t;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

/* USER CODE BEGIN EC */
#if defined (SENSOR_ENABLED) && (SENSOR_ENABLED == 1) && defined (X_NUCLEO_IKS01A2)
#define HTS221_0    0U      /* HTS221 temperature/humidity sensor index */
#define LPS22HB_0   1U      /* LPS22HB pressure sensor index */
#endif
/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initialize environmental sensors
  * @retval 0 on success, error code on failure
  */
int32_t EnvSensors_Init(void);

/**
  * @brief  Read environmental sensor data
  * @param  sensor_data Pointer to sensor data structure
  * @retval 0 on success, error code on failure
  */
int32_t EnvSensors_Read(sensor_t *sensor_data);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H__ */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/