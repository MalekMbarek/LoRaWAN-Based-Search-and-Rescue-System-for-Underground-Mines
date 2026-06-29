/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   Scanner node application main entry
  *          - I2C LCD display
  *          - Joystick input (digital + analog)
  *          - Environmental sensors (HTS221, LPS22HB)
  *          - LoRaWAN communication
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "scanner_app.h"
#include "sys_app.h"
#include "adc.h"
#include "joystick.h"

/* External variables */
extern ADC_HandleTypeDef hadc;
extern I2C_HandleTypeDef hi2c1;

/* Function prototypes */
void SystemClock_Config(void);
static void I2C_Init(void);
static void GPIO_Init(void);
static void ADC_Init(void);

/**
  * @brief  Main application entry point
  * @retval int
  */
int main(void)
{
    /* Reset peripherals, initialize Flash and Systick */
    HAL_Init();
    
    /* Configure system clock */
    SystemClock_Config();
    
    /* Initialize system application */
    SystemApp_Init();
    
    /* Initialize I2C for LCD */
    I2C_Init();
    GPIO_Init();
    ADC_Init();
    
    /* Initialize scanner application */
    Scanner_Init();
    Joystick_SW_Init();
    
    /* Main loop */
    while (1)
    {
        Scanner_Process();
        HAL_Delay(10);
    }
}

/**
  * @brief  Error handler
  * @retval None
  */
void Error_Handler(void)
{
    while (1)
    {
        /* Optional: blink LED here */
    }
}

/**
  * @brief  Initialize I2C1 for LCD display
  * @note   PB8 = SCL, PB9 = SDA
  */
static void I2C_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    
    /* I2C1 GPIO Configuration: PB8 (SCL), PB9 (SDA) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* Configure I2C at 100kHz @ 32MHz */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00707CBB;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

/**
  * @brief  System Clock Configuration
  * @retval None
  * @note   Use existing SystemClock_Config from static2 project
  */
void SystemClock_Config(void)
{
    /* TODO: Implement clock configuration */
}

/**
  * @brief  Initialize GPIO pins
  * @note   PA3 = Joystick button (SW)
  *         PA0, PA2 = Joystick analog inputs
  */
static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Joystick button (SW) - PA3 as input with pull-up */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Joystick analog inputs - PA0, PA2 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  Initialize ADC1 for joystick reading
  * @note   Uses channels 0 (PA0) and 2 (PA2)
  */
static void ADC_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc.Instance = ADC1;

    hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.ScanConvMode = ENABLE;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;

    HAL_ADC_Init(&hadc);

    ADC_ChannelConfTypeDef sConfig = {0};

    /* Configure channel 0 (PA0 - Joystick X-axis) */
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    /* Configure channel 2 (PA2 - Joystick Y-axis) */
    sConfig.Channel = ADC_CHANNEL_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);
}