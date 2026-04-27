/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Scanner application main
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "scanner_app.h"
#include "sys_app.h"

/* Private variables */
extern I2C_HandleTypeDef hi2c1;

/* Function prototypes */
void SystemClock_Config(void);
static void I2C_Init(void);

int main(void)
{
    /* Reset peripherals, initialize Flash and Systick */
    HAL_Init();
    
    /* Configure system clock */
    SystemClock_Config();
    
    /* Initialize system */
    SystemApp_Init();
    
    /* Initialize I2C for LCD */
    I2C_Init();
    
    /* Initialize scanner application */
    Scanner_Init();
    
    /* Main loop */
    while (1)
    {
        Scanner_Process();
        HAL_Delay(10);
    }
}

void Error_Handler(void)
{
    while (1)
    {
        // Optional: blink LED here
    }
}
static void I2C_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    
    /* I2C1 GPIO Configuration: PB8 = SCL, PB9 = SDA */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* Configure I2C */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00707CBB;  // 100 kHz @ 32 MHz
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

void SystemClock_Config(void)
{
    // Use your existing SystemClock_Config from static2
}