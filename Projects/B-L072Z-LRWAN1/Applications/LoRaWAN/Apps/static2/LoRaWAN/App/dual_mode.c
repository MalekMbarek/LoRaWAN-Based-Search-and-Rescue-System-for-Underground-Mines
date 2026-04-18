/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dual_mode.c
  * @brief   Dual mode LoRaWAN/Raw LoRa implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "dual_mode.h"
#include "platform.h"
#include "stm32l0xx_hal.h"  // Include HAL BEFORE utilities.h to avoid macro conflicts
#include <string.h>

// Now include LoRaWAN headers (which include utilities.h with SUCCESS macro)
#include "radio.h"
#include "lora_app.h"
#include "LmHandler.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "stm32_timer.h"
#include "sys_app.h"

/* Raw LoRa Configuration (must match personal node) */
#define RAW_RF_FREQUENCY                868000000
#define RAW_LORA_BANDWIDTH              0         // 125 kHz
#define RAW_LORA_SPREADING_FACTOR       7
#define RAW_LORA_CODINGRATE             1
#define RAW_LORA_PREAMBLE_LENGTH        8
#define RAW_LORA_SYMBOL_TIMEOUT         5
#define RAW_LORA_FIX_LENGTH_PAYLOAD_ON  false
#define RAW_LORA_IQ_INVERSION_ON        false

/* Mode timing - Independent of LoRaWAN state */
#define LORAWAN_MODE_DURATION_MS        60000     // 60s in LoRaWAN mode
#define RAW_LORA_MODE_DURATION_MS       30000     // 30s in Raw LoRa RX mode

/* Private variables */
static OperatingMode_t currentMode = MODE_LORAWAN;
static RadioEvents_t rawRadioEvents;
static UTIL_TIMER_Object_t modeSwitchTimer;
static volatile uint8_t newPacketReceived = 0;
static PersonalNodePacket_t lastPersonalNodeData;
static uint8_t loraWanSuspended = 0;  // Track LoRaWAN state

/* Forward declarations */
static void RawLoRa_Init(void);
static void RawLoRa_StartRx(void);
static void LoRaWAN_Resume(void);
static void LoRaWAN_Suspend(void);
static void OnModeSwitchTimer(void *context);

/* Raw LoRa Radio Callbacks */
static void OnRawRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (currentMode != MODE_RAW_LORA_RX) return;
    
    APP_LOG(TS_OFF, VLEVEL_M, "[RawRX] Packet: size=%d RSSI=%d SNR=%d\r\n", 
            size, rssi, snr);
    
    // Validate packet size
    if (size == sizeof(PersonalNodePacket_t)) {
        memcpy(&lastPersonalNodeData, payload, sizeof(PersonalNodePacket_t));
        
        // Parse accelerometer data (bytes 4-9 are big-endian int16)
        lastPersonalNodeData.accX = (int16_t)((payload[4] << 8) | payload[5]);
        lastPersonalNodeData.accY = (int16_t)((payload[6] << 8) | payload[7]);
        lastPersonalNodeData.accZ = (int16_t)((payload[8] << 8) | payload[9]);
        
        newPacketReceived = 1;
        
        // Log received data
        const char* statusStr[] = {"OK", "FALL", "SOS"};
        APP_LOG(TS_OFF, VLEVEL_M, "[RawRX] Node=%d Status=%s HR=%d Acc[%d,%d,%d]\r\n",
                lastPersonalNodeData.nodeId,
                statusStr[lastPersonalNodeData.status],
                lastPersonalNodeData.heartRate,
                lastPersonalNodeData.accX,
                lastPersonalNodeData.accY,
                lastPersonalNodeData.accZ);
        
        // Handle alerts
        if (lastPersonalNodeData.status == 2) {  // STATUS_SOS
            APP_LOG(TS_OFF, VLEVEL_M, "!!! PERSONAL NODE SOS ALERT !!!\r\n");
            for (int i = 0; i < 6; i++) {
                LED_On(LED_RED1);
                HAL_Delay(100);
                LED_Off(LED_RED1);
                HAL_Delay(100);
            }
        }
        else if (lastPersonalNodeData.status == 1) {  // STATUS_FALL
            APP_LOG(TS_OFF, VLEVEL_M, "!!! PERSONAL NODE FALL DETECTED !!!\r\n");
            for (int i = 0; i < 10; i++) {
                LED_On(LED_RED1);
                HAL_Delay(80);
                LED_Off(LED_RED1);
                HAL_Delay(80);
            }
        }
    }
    
    // Continue receiving
    RawLoRa_StartRx();
}

static void OnRawRxTimeout(void)
{
    if (currentMode != MODE_RAW_LORA_RX) return;
    RawLoRa_StartRx();
}

static void OnRawRxError(void)
{
    if (currentMode != MODE_RAW_LORA_RX) return;
    APP_LOG(TS_OFF, VLEVEL_L, "[RawRX] Error, restarting\r\n");
    RawLoRa_StartRx();
}

static void OnRawTxDone(void)
{
    Radio.Sleep();
}

static void OnRawTxTimeout(void)
{
    Radio.Sleep();
}

/* Raw LoRa Functions */
static void RawLoRa_Init(void)
{
    // Setup radio callbacks
    rawRadioEvents.TxDone = OnRawTxDone;
    rawRadioEvents.TxTimeout = OnRawTxTimeout;
    rawRadioEvents.RxDone = OnRawRxDone;
    rawRadioEvents.RxTimeout = OnRawRxTimeout;
    rawRadioEvents.RxError = OnRawRxError;
    
    Radio.Init(&rawRadioEvents);
    Radio.SetChannel(RAW_RF_FREQUENCY);
    
    // Configure RX mode
    Radio.SetRxConfig(MODEM_LORA,
                      RAW_LORA_BANDWIDTH,
                      RAW_LORA_SPREADING_FACTOR,
                      RAW_LORA_CODINGRATE,
                      0,  // AFC bandwidth (not used in LoRa)
                      RAW_LORA_PREAMBLE_LENGTH,
                      RAW_LORA_SYMBOL_TIMEOUT,
                      RAW_LORA_FIX_LENGTH_PAYLOAD_ON,
                      0,  // Fixed payload length (0 = variable)
                      true,  // CRC enabled
                      0,
                      0,
                      RAW_LORA_IQ_INVERSION_ON,
                      true);  // Continuous RX
    
    APP_LOG(TS_OFF, VLEVEL_M, "[RawLoRa] Init complete\r\n");
}

static void RawLoRa_StartRx(void)
{
    Radio.Rx(0);  // 0 = continuous RX mode
}

/* LoRaWAN Functions */
static void LoRaWAN_Suspend(void)
{
    if (loraWanSuspended) return;
    
    // Put radio to sleep - stops all LoRaWAN operations
    Radio.Sleep();
    loraWanSuspended = 1;
    
    APP_LOG(TS_OFF, VLEVEL_M, "[LoRaWAN] Suspended (join state preserved)\r\n");
}

static void LoRaWAN_Resume(void)
{
    if (!loraWanSuspended) return;
    
    // LoRaWAN stack will reconfigure radio on next process
    // Join state is preserved in LmHandler
    loraWanSuspended = 0;
    
    APP_LOG(TS_OFF, VLEVEL_M, "[LoRaWAN] Resumed (will continue from previous state)\r\n");
}

/* Mode switching - Time-based, independent of network state */
static void OnModeSwitchTimer(void *context)
{
    // Auto-switch between modes based on timer
    if (currentMode == MODE_LORAWAN) {
        DualMode_Switch(MODE_RAW_LORA_RX);
    } else {
        DualMode_Switch(MODE_LORAWAN);
    }
}

void DualMode_Init(void)
{
    currentMode = MODE_LORAWAN;
    loraWanSuspended = 0;
    
    // Create mode switch timer for automatic switching
    UTIL_TIMER_Create(&modeSwitchTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_ONESHOT,  // One-shot, will restart with new period
                      OnModeSwitchTimer, 
                      NULL);
    
    // Start with LoRaWAN mode duration
    UTIL_TIMER_SetPeriod(&modeSwitchTimer, LORAWAN_MODE_DURATION_MS);
    UTIL_TIMER_Start(&modeSwitchTimer);
    
    APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] Init: LoRaWAN=%ds RawRX=%ds\r\n",
            LORAWAN_MODE_DURATION_MS/1000, RAW_LORA_MODE_DURATION_MS/1000);
}

void DualMode_Switch(OperatingMode_t newMode)
{
    if (currentMode == newMode) {
        APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] Already in requested mode\r\n");
        return;
    }
    
    uint32_t nextModeDuration;
    
    if (newMode == MODE_RAW_LORA_RX) {
        APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] LoRaWAN -> RawRX\r\n");
        
        // Suspend LoRaWAN (doesn't reset join state)
        LoRaWAN_Suspend();
        
        // Small delay for radio state machine
        HAL_Delay(50);
        
        // Initialize and start Raw LoRa RX
        RawLoRa_Init();
        RawLoRa_StartRx();
        
        currentMode = MODE_RAW_LORA_RX;
        nextModeDuration = RAW_LORA_MODE_DURATION_MS;
        
        // Visual indication - Blue LED on in RX mode
        LED_On(LED_BLUE);
        
    } else {  // MODE_LORAWAN
        APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] RawRX -> LoRaWAN\r\n");
        
        // Stop Raw LoRa
        Radio.Sleep();
        
        // Turn off RX mode LED
        LED_Off(LED_BLUE);
        
        // Small delay for radio state machine
        HAL_Delay(50);
        
        // Resume LoRaWAN (continues from previous state - joined or joining)
        LoRaWAN_Resume();
        
        currentMode = MODE_LORAWAN;
        nextModeDuration = LORAWAN_MODE_DURATION_MS;
    }
    
    // Restart timer with new duration for next mode
    UTIL_TIMER_Stop(&modeSwitchTimer);
    UTIL_TIMER_SetPeriod(&modeSwitchTimer, nextModeDuration);
    UTIL_TIMER_Start(&modeSwitchTimer);
    
    APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] Next switch in %ds\r\n", nextModeDuration/1000);
}

OperatingMode_t DualMode_GetCurrent(void)
{
    return currentMode;
}

uint8_t DualMode_IsLoRaWANSuspended(void)
{
    return loraWanSuspended;
}

void DualMode_Process(void)
{
    // Process received personal node data
    if (currentMode == MODE_RAW_LORA_RX && newPacketReceived) {
        newPacketReceived = 0;
        
        // Optional: Switch to LoRaWAN immediately on critical alerts
        // Uncomment if you want immediate LoRaWAN uplink on fall/SOS
        /*
        if (lastPersonalNodeData.status != 0) {  // FALL or SOS
            APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] Alert detected, switching to LoRaWAN\r\n");
            DualMode_Switch(MODE_LORAWAN);
            
            // Trigger immediate uplink
            UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
        }
        */
    }
}

/* Get last received personal node data */
PersonalNodePacket_t* DualMode_GetLastPersonalNodeData(void)
{
    return &lastPersonalNodeData;
}