/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dual_mode.c
  * @brief   Dual mode LoRaWAN/Raw LoRa implementation for static node
  *
  * @note    Alternates between LoRaWAN mode (30s) and Raw LoRa RX mode (30s)
  *          Receives and stores data from personal nodes
  ******************************************************************************
  */
/* USER CODE END Header */

#include "dual_mode.h"
#include "platform.h"
#include "stm32l0xx_hal.h"
#include <string.h>

#include "radio.h"
#include "lora_app.h"
#include "LmHandler.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "stm32_timer.h"
#include "sys_app.h"
#include <math.h>

/* ============================================================
 * Raw LoRa Configuration
 * Must match personal node settings
 * ============================================================ */
#define RAW_RF_FREQUENCY                868000000
#define RAW_LORA_BANDWIDTH              0
#define RAW_LORA_SPREADING_FACTOR       7
#define RAW_LORA_CODINGRATE             1
#define RAW_LORA_PREAMBLE_LENGTH        8
#define RAW_LORA_SYMBOL_TIMEOUT         5
#define RAW_LORA_FIX_LENGTH_PAYLOAD_ON  false
#define RAW_LORA_IQ_INVERSION_ON        false

/* ============================================================
 * Mode Timing
 * ============================================================ */
#define LORAWAN_MODE_DURATION_MS        30000     /* 30s in LoRaWAN mode */
#define RAW_LORA_MODE_DURATION_MS       30000     /* 30s in Raw LoRa RX mode */

/* ============================================================
 * Private Variables
 * ============================================================ */
static OperatingMode_t currentMode = MODE_LORAWAN;
static RadioEvents_t rawRadioEvents;
static UTIL_TIMER_Object_t modeSwitchTimer;
static volatile uint8_t newPacketReceived = 0;
static PersonalNodePacket_t lastPersonalNodeData;
static uint8_t loraWanSuspended = 0;
static uint8_t loraWanJoined = 0;
static uint8_t alertPending = 0;

/* Storage for personal nodes data */
static StoredPersonalNode_t storedNodes[MAX_PERSONAL_NODES];

/* ============================================================
 * Forward Declarations
 * ============================================================ */
static void RawLoRa_ConfigureRx(void);
static void RawLoRa_StartRx(void);
static void LoRaWAN_Resume(void);
static void LoRaWAN_Suspend(void);
static void OnModeSwitchTimer(void *context);
static void StorePersonalNodeData(PersonalNodePacket_t* packet);
static float CalculateDistanceFromRSSI(int16_t rssi);

/* ============================================================
 * Public Functions
 * ============================================================ */

void DualMode_SetAlertPending(uint8_t pending)
{
    alertPending = pending;
}

uint8_t DualMode_IsAlertPending(void)
{
    return alertPending;
}

/* ============================================================
 * Private Functions
 * ============================================================ */

/**
  * @brief Calculate distance from RSSI using log-distance path loss model
  * @param rssi Received signal strength in dBm
  * @return Estimated distance in meters
  * @note  Formula: d = 10^((TxPower - RSSI) / (10 * n))
  *        TxPower ≈ -40 dBm at 1m, n = 3.0 (indoor/mine environment)
  */
static float CalculateDistanceFromRSSI(int16_t rssi)
{
    const float txPower = -40.0f;   /* Tx power at 1m (dBm) */
    const float n = 3.0f;            /* Path loss exponent (indoor/mine) */

    float distance = powf(10.0f, (txPower - rssi) / (10.0f * n));

    if (distance < 1.0f)
        distance = 1.0f;

    return distance;
}

/* ============================================================
 * Raw LoRa Radio Callbacks
 * ============================================================ */

/**
  * @brief Raw LoRa RX done callback
  * @param payload Received data buffer
  * @param size Payload size in bytes
  * @param rssi RSSI of received packet
  * @param snr SNR of received packet
  */
static void OnRawRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (currentMode != MODE_RAW_LORA_RX) return;

    APP_LOG(TS_OFF, VLEVEL_M,
            "[RawRX] Packet: size=%d RSSI=%d SNR=%d\r\n",
            size, rssi, snr);

    if (size < 11) {
        RawLoRa_StartRx();
        return;
    }

    /* Parse packet */
    lastPersonalNodeData.pktType   = payload[0];
    lastPersonalNodeData.nodeId    = payload[1];
    lastPersonalNodeData.status    = payload[2];
    lastPersonalNodeData.heartRate = payload[3];

    lastPersonalNodeData.accX = (int16_t)((int16_t)payload[4] << 8 | payload[5]);
    lastPersonalNodeData.accY = (int16_t)((int16_t)payload[6] << 8 | payload[7]);
    lastPersonalNodeData.accZ = (int16_t)((int16_t)payload[8] << 8 | payload[9]);
    lastPersonalNodeData.spo2 = payload[10];

    lastPersonalNodeData.rssi = rssi;
    lastPersonalNodeData.distance = CalculateDistanceFromRSSI(rssi);

    StorePersonalNodeData(&lastPersonalNodeData);

    newPacketReceived = 1;

    const char* statusStr[] = {"OK", "FALL", "SOS"};

    int d_int = (int)lastPersonalNodeData.distance;
    int d_dec = (int)((lastPersonalNodeData.distance - d_int) * 100);
    if (d_dec < 0) d_dec = -d_dec;

    APP_LOG(TS_OFF, VLEVEL_M,
            "[RawRX] Node=%d Status=%s HR=%d SpO2=%d Dist=%d.%02d m Acc[%d,%d,%d]\r\n",
            lastPersonalNodeData.nodeId,
            statusStr[lastPersonalNodeData.status],
            lastPersonalNodeData.heartRate,
            lastPersonalNodeData.spo2,
            d_int, d_dec,
            lastPersonalNodeData.accX,
            lastPersonalNodeData.accY,
            lastPersonalNodeData.accZ);

    /* Switch to LoRaWAN immediately on alert */
    if (lastPersonalNodeData.status == STATUS_FALL ||
        lastPersonalNodeData.status == STATUS_SOS)
    {
        DualMode_SetAlertPending(1);
        DualMode_Switch(MODE_LORAWAN);
        return;
    }

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

/* ============================================================
 * Personal Node Storage Functions
 * ============================================================ */

/**
  * @brief Store personal node data in array
  * @param packet Personal node packet to store
  */
static void StorePersonalNodeData(PersonalNodePacket_t* packet)
{
    if (packet->nodeId < 1 || packet->nodeId > MAX_PERSONAL_NODES) {
        APP_LOG(TS_OFF, VLEVEL_M, "[Storage] Invalid node ID: %d\r\n", packet->nodeId);
        return;
    }
    
    uint8_t index = packet->nodeId - 1;
    
    storedNodes[index].nodeId = packet->nodeId;
    storedNodes[index].status = packet->status;
    storedNodes[index].heartRate = packet->heartRate;
    storedNodes[index].spo2 = packet->spo2;
    storedNodes[index].accX = packet->accX;
    storedNodes[index].accY = packet->accY;
    storedNodes[index].accZ = packet->accZ;
    storedNodes[index].rssi = packet->rssi;
    storedNodes[index].distance = packet->distance;
    storedNodes[index].lastUpdateTime = HAL_GetTick();
    storedNodes[index].isActive = 1;

    if (packet->status == STATUS_FALL || packet->status == STATUS_SOS) {
        storedNodes[index].hasAlert = 1;
    }
    
    APP_LOG(TS_OFF, VLEVEL_L, "[Storage] Node %d updated\r\n", packet->nodeId);
}

/* ============================================================
 * Raw LoRa Functions
 * ============================================================ */

static void RawLoRa_ConfigureRx(void)
{
    /* Setup raw radio callbacks */
    rawRadioEvents.TxDone = OnRawTxDone;
    rawRadioEvents.TxTimeout = OnRawTxTimeout;
    rawRadioEvents.RxDone = OnRawRxDone;
    rawRadioEvents.RxTimeout = OnRawRxTimeout;
    rawRadioEvents.RxError = OnRawRxError;
    
    /* Reinit radio with raw callbacks */
    Radio.Init(&rawRadioEvents);
    Radio.SetChannel(RAW_RF_FREQUENCY);
    
    /* Configure RX mode */
    Radio.SetRxConfig(MODEM_LORA,
                      RAW_LORA_BANDWIDTH,
                      RAW_LORA_SPREADING_FACTOR,
                      RAW_LORA_CODINGRATE,
                      0,
                      RAW_LORA_PREAMBLE_LENGTH,
                      RAW_LORA_SYMBOL_TIMEOUT,
                      RAW_LORA_FIX_LENGTH_PAYLOAD_ON,
                      0,
                      true,
                      0,
                      0,
                      RAW_LORA_IQ_INVERSION_ON,
                      true);  /* Continuous RX */
    
    APP_LOG(TS_OFF, VLEVEL_M, "[RawLoRa] RX configured\r\n");
}

static void RawLoRa_StartRx(void)
{
    Radio.Rx(0);  /* 0 = continuous RX mode */
}

/* ============================================================
 * LoRaWAN Functions
 * ============================================================ */

/**
  * @brief Suspend LoRaWAN operation
  * @note  Stops LoRaWAN and leaves network
  */
static void LoRaWAN_Suspend(void)
{
    if (loraWanSuspended) return;
    
    LmHandlerStop();
    Radio.Sleep();
    
    loraWanSuspended = 1;
    loraWanJoined = 0;
    
    APP_LOG(TS_OFF, VLEVEL_M, "[LoRaWAN] Stopped and left network\r\n");
}

/**
  * @brief Resume LoRaWAN operation
  * @note  Restarts LoRaWAN stack and rejoins network
  */
static void LoRaWAN_Resume(void)
{
    if (!loraWanSuspended) return;
    
    Radio.Sleep();
    HAL_Delay(200);
    
    LoRaWAN_Restart();
    
    loraWanSuspended = 0;
    APP_LOG(TS_OFF, VLEVEL_M, "[LoRaWAN] Resumed and rejoining\r\n");
}

/* ============================================================
 * Mode Switching
 * ============================================================ */

/**
  * @brief Auto-switch timer callback
  * @param context Timer context (unused)
  */
static void OnModeSwitchTimer(void *context)
{
    if (currentMode == MODE_LORAWAN) {
        DualMode_Switch(MODE_RAW_LORA_RX);
    } else {
        DualMode_Switch(MODE_LORAWAN);
    }
}

/* ============================================================
 * Dual Mode Public API
 * ============================================================ */

void DualMode_Init(void)
{
    currentMode = MODE_LORAWAN;
    loraWanSuspended = 0;
    loraWanJoined = 0;
    
    memset(storedNodes, 0, sizeof(storedNodes));
    
    UTIL_TIMER_Create(&modeSwitchTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_ONESHOT,
                      OnModeSwitchTimer, 
                      NULL);
    
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
        
        LoRaWAN_Suspend();
        HAL_Delay(200);
        
        RawLoRa_ConfigureRx();
        RawLoRa_StartRx();
        
        currentMode = MODE_RAW_LORA_RX;
        nextModeDuration = RAW_LORA_MODE_DURATION_MS;
        
        LED_On(LED_BLUE);
        
    } else {  /* MODE_LORAWAN */
        APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] RawRX -> LoRaWAN\r\n");
        
        Radio.Sleep();
        LED_Off(LED_BLUE);
        HAL_Delay(200);
        
        LoRaWAN_Resume();
        
        currentMode = MODE_LORAWAN;
        nextModeDuration = LORAWAN_MODE_DURATION_MS;
    }
    
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

void DualMode_SetJoinStatus(uint8_t joined)
{
    loraWanJoined = joined;
    if (joined) {
        APP_LOG(TS_OFF, VLEVEL_M, "[DualMode] LoRaWAN joined successfully\r\n");
    }
}

uint8_t DualMode_IsLoRaWANJoined(void)
{
    return loraWanJoined;
}

void DualMode_Process(void)
{
    /* Node timeout checking - currently disabled */
}

PersonalNodePacket_t* DualMode_GetLastPersonalNodeData(void)
{
    return &lastPersonalNodeData;
}

StoredPersonalNode_t* DualMode_GetStoredNode(uint8_t nodeId)
{
    if (nodeId < 1 || nodeId > MAX_PERSONAL_NODES) {
        return NULL;
    }
    return &storedNodes[nodeId - 1];
}

uint8_t DualMode_GetActiveNodeCount(void)
{
    uint8_t count = 0;
    for (int i = 0; i < MAX_PERSONAL_NODES; i++) {
        if (storedNodes[i].isActive) {
            count++;
        }
    }
    return count;
}

void DualMode_GetAllActiveNodes(StoredPersonalNode_t* nodes, uint8_t* count)
{
    *count = 0;
    for (int i = 0; i < MAX_PERSONAL_NODES; i++) {
        if (storedNodes[i].isActive) {
            memcpy(&nodes[*count], &storedNodes[i], sizeof(StoredPersonalNode_t));
            (*count)++;
        }
    }
}

uint8_t DualMode_HasAnyAlerts(void)
{
    for (int i = 0; i < MAX_PERSONAL_NODES; i++) {
        if (storedNodes[i].isActive && storedNodes[i].hasAlert) {
            return 1;
        }
    }
    return 0;
}

void DualMode_ClearAlert(uint8_t nodeId)
{
    if (nodeId < 1 || nodeId > MAX_PERSONAL_NODES) {
        return;
    }
    storedNodes[nodeId - 1].hasAlert = 0;
}