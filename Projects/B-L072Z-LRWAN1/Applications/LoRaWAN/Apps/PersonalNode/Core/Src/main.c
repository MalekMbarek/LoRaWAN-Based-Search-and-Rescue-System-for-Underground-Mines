/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c — PersonalNode
  * @brief          : Main application entry point
  *
  * @note           : All sensor reading, display, and periodic tasks are handled
  *                   by UTIL_TIMER_PERIODIC callbacks in lora_app.c.
  *                   main.c only initializes the system and runs the sequencer.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_lorawan.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sys_app.h"
#include "lora_app.h"
#include "app_config.h"
/* USER CODE END Includes */

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
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /*
   * MX_LoRaWAN_Init() performs:
   *   - SystemApp_Init() : UART logging, LPM, sequencer, RTC
   *   - GPIO_PersonalNode_Init() : PA0 (SOS), PA5 & PB13 (LEDs)
   *   - LoRaWAN_Init() : Radio init, SensorsInit, periodic timers
   */
  MX_LoRaWAN_Init();

  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /*
     * MX_LoRaWAN_Process() calls UTIL_SEQ_Run(~0U) which executes:
     *   - AccTask     : AccRead + Fall_Update (every 50ms)
     *   - HrTask      : HrRead (every 10ms)
     *   - DisplayTask : DisplaySensorData (every 3s)
     *   - TxTask      : Periodic LoRa transmission (every 10s)
     *
     * The MCU enters stop mode (LPM) between tasks.
     * RTC wakes the MCU on each timer expiration.
     */
    MX_LoRaWAN_Process();
   
    /* USER CODE BEGIN 3 */
    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState            = RCC_HSE_OFF;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLLMUL_6;
  RCC_OscInitStruct.PLL.PLLDIV          = RCC_PLLDIV_3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOS) != RESET) {}

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                     RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) { Error_Handler(); }
}

/* USER CODE BEGIN 4 */

/* SOS Button — EXTI callback (commented out, handled by timer tasks) */
/*
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == SOS_BUTTON_PIN) {
    HAL_GPIO_WritePin(LED_SOS_PORT, LED_SOS_PIN, GPIO_PIN_SET);
    for (volatile int i = 0; i < 200000; i++);
    HAL_GPIO_WritePin(LED_SOS_PORT, LED_SOS_PIN, GPIO_PIN_RESET);
    PN_SOS_Trigger();
  }
}
*/

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while (1) {} }
#endif