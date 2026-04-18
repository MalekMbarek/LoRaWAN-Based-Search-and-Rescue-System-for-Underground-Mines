/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_rawlora.c
  * @author  Custom Implementation
  * @brief   Application for Raw LoRa frame reception
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_rawlora.h"
#include "rawlora_app.h"
#include "sys_app.h"
#include "stm32_seq.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions --------------------------------------------------------*/

void MX_RawLoRa_Init(void)
{
  /* USER CODE BEGIN MX_RawLoRa_Init_1 */

  /* USER CODE END MX_RawLoRa_Init_1 */
    SystemApp_Init();

  /* Initialize Raw LoRa application */
  RawLoRa_Init();
  
  /* USER CODE BEGIN MX_RawLoRa_Init_2 */

  /* USER CODE END MX_RawLoRa_Init_2 */
}

void MX_RawLoRa_Process(void)
{
  /* USER CODE BEGIN MX_RawLoRa_Process_1 */

  /* USER CODE END MX_RawLoRa_Process_1 */
  
  /* Process Raw LoRa reception */
  RawLoRa_Process();
  
  /* USER CODE BEGIN MX_RawLoRa_Process_2 */

  /* USER CODE END MX_RawLoRa_Process_2 */
}

void MX_RawLoRa_DeInit(void)
{
  /* USER CODE BEGIN MX_RawLoRa_DeInit_1 */

  /* USER CODE END MX_RawLoRa_DeInit_1 */
  
  /* Deinitialize Raw LoRa application */
  RawLoRa_DeInit();
  
  /* USER CODE BEGIN MX_RawLoRa_DeInit_2 */

  /* USER CODE END MX_RawLoRa_DeInit_2 */
}

/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Private Functions Definition -----------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/