/**
 * @file    main.c
 * @brief   Personal node — Fall detection + SOS + Heart rate -> raw LoRa TX
 *          Uses ST HAL + SubGHz_Phy radio driver (sx1276)
 *          Logs over UART2 (PA2/PA3) at 115200 baud
 */
#include "main.h"
#include "sx1276.h"
#include "packet.h"   // define your packet constants
#include <string.h>
#include <stdio.h>

// Pin definitions for B-L072Z-LRWAN1 (adjust if needed)
#define RADIO_RESET_PIN      GPIO_PIN_13
#define RADIO_RESET_PORT     GPIOA
#define RADIO_NSS_PIN        GPIO_PIN_12
#define RADIO_NSS_PORT       GPIOB
#define RADIO_DIO0_PIN       GPIO_PIN_11   // PB11
#define RADIO_DIO0_PORT      GPIOB

// SPI2 handle (initialise in main)
extern SPI_HandleTypeDef hspi2;

// Radio callbacks (simple)
static void OnTxDone(void)    { /* toggle LED or nothing */ }
static void OnTxTimeout(void) { /* handle timeout */ }

// SX1276 handle
static SX1276_t sx1276;

// Forward declaration
static void Radio_Init(void);

// Your other variables (sosTriggered, etc.)
volatile uint8_t sosTriggered = 0;

int main(void)
{
    HAL_Init();
    SystemClock_Config();   // you must provide this or use HAL_RCC_* directly
    GPIO_Init();            // your LED, button init
    UART_Init();            // for logging
    I2C_Init();             // for sensors

    Radio_Init();

    // ... sensor init, loop as before, but use sx1276.Send() instead of Radio.Send()
    while (1)
    {
        // ... same logic as your original main.c, but replace:
        // Radio.Send(payload, len);  ->  SX1276Send(&sx1276, payload, len);
        // Also need to handle TxDone/TxTimeout via callbacks or polling
    }
}

static void Radio_Init(void)
{
    // Initialise SPI2, GPIO for NSS, RESET, DIO0
    // This is board‑specific; use the BSP or do it manually.
    // For B-L072Z-LRWAN1, you can copy from ST's radio_board.c.
    // Here's a minimal example – you may need to adapt.

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // Configure NSS (PB12) as output
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = RADIO_NSS_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RADIO_NSS_PORT, &gpio);
    HAL_GPIO_WritePin(RADIO_NSS_PORT, RADIO_NSS_PIN, GPIO_PIN_SET);

    // Configure RESET (PA13) as output
    gpio.Pin = RADIO_RESET_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(RADIO_RESET_PORT, &gpio);
    HAL_GPIO_WritePin(RADIO_RESET_PORT, RADIO_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RADIO_RESET_PORT, RADIO_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    // Configure DIO0 (PB11) as input (for TxDone interrupt, optional)
    gpio.Pin = RADIO_DIO0_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(RADIO_DIO0_PORT, &gpio);

    // SPI2 init (pins PB13=SCK, PB14=MISO, PB15=MOSI)
    gpio.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&hspi2);

    // Initialise SX1276 structure
    sx1276.Reset = Radio_Reset;   // you need to implement these callbacks
    sx1276.SpiReadWrite = Radio_SpiReadWrite;
    sx1276.GetTick = HAL_GetTick;
    sx1276.DelayMs = HAL_Delay;

    SX1276Init(&sx1276, MODEM_LORA);
    SX1276SetChannel(&sx1276, 868000000);
    SX1276SetTxConfig(&sx1276, MODEM_LORA, 14, 0, 0, 7, 1, 8, false, true, 0, 0, false, 3000);
}

// Implement the low-level callbacks
static uint8_t Radio_SpiReadWrite(uint8_t data)
{
    uint8_t rx;
    HAL_GPIO_WritePin(RADIO_NSS_PORT, RADIO_NSS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, 100);
    HAL_GPIO_WritePin(RADIO_NSS_PORT, RADIO_NSS_PIN, GPIO_PIN_SET);
    return rx;
}

static void Radio_Reset(void)
{
    HAL_GPIO_WritePin(RADIO_RESET_PORT, RADIO_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(RADIO_RESET_PORT, RADIO_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}