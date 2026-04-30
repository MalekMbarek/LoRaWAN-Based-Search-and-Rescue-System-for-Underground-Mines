/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scanner_app.c
  * @brief   Scanner application implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "scanner_app.h"
#include "platform.h"
#include "stm32l0xx_hal.h"
#include "radio.h"
#include "sys_app.h"
#include "stm32_timer.h"
#include "lcd_display.h"
#include "buzzer.h"
#include <string.h>
#include <stdio.h>
#include "joystick.h"
/* Raw LoRa Configuration (must match personal nodes) */
#define RAW_RF_FREQUENCY                868000000
#define RAW_LORA_BANDWIDTH              0         // 125 kHz
#define RAW_LORA_SPREADING_FACTOR       7
#define RAW_LORA_CODINGRATE             1
#define RAW_LORA_PREAMBLE_LENGTH        8
#define RAW_LORA_SYMBOL_TIMEOUT         5
#define RAW_LORA_FIX_LENGTH_PAYLOAD_ON  false
#define RAW_LORA_IQ_INVERSION_ON        false

/* Node timeout - consider inactive after 10 seconds */
#define NODE_TIMEOUT_MS                 10000

/* Update intervals */
#define LCD_UPDATE_INTERVAL_MS          500
#define BUZZER_UPDATE_INTERVAL_MS       100
extern ADC_HandleTypeDef hadc;
/* Private variables */
static uint8_t uiMode;
//static void Read_Joystick(void);
static uint8_t uiMode = 0;
static uint8_t selectedNodeIndex = 0;

static RadioEvents_t radioEvents;
static TrackedNode_t trackedNodes[MAX_TRACKED_NODES];
static UTIL_TIMER_Object_t lcdUpdateTimer;
static UTIL_TIMER_Object_t buzzerTimer;
static uint8_t buzzerActive = 0;
static uint32_t nextBuzzerTime = 0;
static uint8_t scrollIndex = 0;
static UI_State_t uiState = UI_LIST_VIEW;
/* Forward declarations */
static void Radio_ConfigureRx(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnRxTimeout(void);
static void OnRxError(void);
static void ProcessReceivedPacket(PersonalNodePacket_t* packet, int16_t rssi, int8_t snr);
static void UpdateTrackedNode(PersonalNodePacket_t* packet, int16_t rssi, int8_t snr);
static void OnLcdUpdateTimer(void *context);
static void OnBuzzerTimer(void *context);
static void UpdateBuzzer(void);
static void UpdateLcdDisplay(void);

/* Radio callbacks */
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    APP_LOG(TS_OFF, VLEVEL_L, "[RX] Packet received: size=%d RSSI=%d SNR=%d\r\n", 
            size, rssi, snr);
    
    if (size >= 3) {  // Minimum valid packet
        PersonalNodePacket_t packet;
        memcpy(&packet, payload, sizeof(PersonalNodePacket_t));
        
        // Parse accelerometer data (big-endian)
        packet.accX = (int16_t)((payload[4] << 8) | payload[5]);
        packet.accY = (int16_t)((payload[6] << 8) | payload[7]);
        packet.accZ = (int16_t)((payload[8] << 8) | payload[9]);
        
        ProcessReceivedPacket(&packet, rssi, snr);
    }
    
    // Continue receiving
    Radio.Rx(0);
}

static void OnRxTimeout(void)
{
    Radio.Rx(0);  // Restart RX
}

static void OnRxError(void)
{
    APP_LOG(TS_OFF, VLEVEL_M, "[RX] Error\r\n");
    Radio.Rx(0);  // Restart RX
}

static void OnTxDone(void)
{
    Radio.Sleep();
}

static void OnTxTimeout(void)
{
    Radio.Sleep();
}

/* Process received packet */
static void ProcessReceivedPacket(PersonalNodePacket_t* packet, int16_t rssi, int8_t snr)
{
    if (packet->nodeId < 1 || packet->nodeId > MAX_TRACKED_NODES) {
        APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Invalid node ID: %d\r\n", packet->nodeId);
        return;
    }
    
    UpdateTrackedNode(packet, rssi, snr);
    
    APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Node %d: Status=%s RSSI=%d SNR=%d HR=%d\r\n",
            packet->nodeId,
            Scanner_StatusToString((NodeStatus_t)packet->status),
            rssi, snr, packet->heartRate);
}

static void UpdateTrackedNode(PersonalNodePacket_t* packet, int16_t rssi, int8_t snr)
{
    uint8_t index = packet->nodeId - 1;
    
    trackedNodes[index].nodeId = packet->nodeId;
    trackedNodes[index].status = (NodeStatus_t)packet->status;
    trackedNodes[index].heartRate = packet->heartRate;
    trackedNodes[index].accX = packet->accX;
    trackedNodes[index].accY = packet->accY;
    trackedNodes[index].accZ = packet->accZ;
    trackedNodes[index].rssi = rssi;
    trackedNodes[index].snr = snr;
    trackedNodes[index].proximity = Scanner_RssiToProximity(rssi);
    trackedNodes[index].lastSeenTime = HAL_GetTick();
    trackedNodes[index].isActive = 1;
    trackedNodes[index].packetCount++;
}

/* Radio configuration */
static void Radio_ConfigureRx(void)
{
    radioEvents.TxDone = OnTxDone;
    radioEvents.TxTimeout = OnTxTimeout;
    radioEvents.RxDone = OnRxDone;
    radioEvents.RxTimeout = OnRxTimeout;
    radioEvents.RxError = OnRxError;
    
    Radio.Init(&radioEvents);
    Radio.SetChannel(RAW_RF_FREQUENCY);
    
    Radio.SetRxConfig(MODEM_LORA,
                      RAW_LORA_BANDWIDTH,
                      RAW_LORA_SPREADING_FACTOR,
                      RAW_LORA_CODINGRATE,
                      0,
                      RAW_LORA_PREAMBLE_LENGTH,
                      RAW_LORA_SYMBOL_TIMEOUT,
                      RAW_LORA_FIX_LENGTH_PAYLOAD_ON,
                      0,
                      true,  // CRC enabled
                      0,
                      0,
                      RAW_LORA_IQ_INVERSION_ON,
                      true);  // Continuous RX
    
    APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Radio configured\r\n");
}

/* Timer callbacks */
static void OnLcdUpdateTimer(void *context)
{
    UpdateLcdDisplay();
}

static void OnBuzzerTimer(void *context)
{
    UpdateBuzzer();
}

/* Buzzer control based on closest urgent node */
static void UpdateBuzzer(void)
{
    uint32_t now = HAL_GetTick();
    
    // Find most urgent node
    uint8_t urgentNodeId = Scanner_GetMostUrgentNode();
    
    if (urgentNodeId == 0) {
        // No urgent nodes - silence buzzer
        Buzzer_Off();
        buzzerActive = 0;
        return;
    }
    
    // Get the urgent node
    TrackedNode_t* node = Scanner_GetNode(urgentNodeId);
    if (!node || !node->isActive) {
        Buzzer_Off();
        buzzerActive = 0;
        return;
    }
    
    // Determine buzzer pattern based on proximity
    uint32_t period;
    switch (node->proximity) {
        case PROXIMITY_VERY_CLOSE:
            period = BUZZ_VERY_CLOSE_PERIOD;
            break;
        case PROXIMITY_CLOSE:
            period = BUZZ_CLOSE_PERIOD;
            break;
        case PROXIMITY_MEDIUM:
            period = BUZZ_MEDIUM_PERIOD;
            break;
        case PROXIMITY_FAR:
            period = BUZZ_FAR_PERIOD;
            break;
        default:
            Buzzer_Off();
            buzzerActive = 0;
            return;
    }
    
    // Toggle buzzer
    if (now >= nextBuzzerTime) {
        if (buzzerActive) {
            Buzzer_Off();
            buzzerActive = 0;
            nextBuzzerTime = now + period;
        } else {
            Buzzer_Beep(50);  // 50ms beep
            buzzerActive = 1;
            nextBuzzerTime = now + 50;
        }
    }
}

/* LCD display update */
static void UpdateLcdDisplay(void)
{
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;

    Scanner_GetAllActiveNodes(nodes, &count);

    LCD_Clear();

    if (count == 0)
    {
        LCD_SetCursor(0,0);
        LCD_Print("SCANNING...");
        LCD_SetCursor(1,0);
        LCD_Print("No nodes");
        return;
    }

    // ================= LIST MODE =================
    if (uiMode == 0)
    {
        for (uint8_t i = 0; i < 2; i++)
        {
            uint8_t idx = scrollIndex + i;

            if (idx >= count) break;

            char line[17];
            snprintf(line, 16, "N%d %s %ddB   AAA",
                     nodes[idx].nodeId,
                     Scanner_StatusToString(nodes[idx].status),
                     nodes[idx].rssi);
            snprintf(line, 16, "N%d %s %ddB",
                     nodes[idx].nodeId,
                     Scanner_StatusToString(nodes[idx].status),
                     nodes[idx].rssi);

            LCD_SetCursor(i,0);
            LCD_Print(line);
        }
    }

    // ================= DETAIL MODE =================
    else
    {
        uint8_t id = selectedNodeIndex;

        TrackedNode_t* n = &nodes[id];

        char l1[17];
        char l2[17];

        snprintf(l1,16,"N%d %s",
                 n->nodeId,
                 Scanner_StatusToString(n->status));

        snprintf(l2,16,"HR:%d RSSI:%d",
                 n->heartRate,
                 n->rssi);

        LCD_SetCursor(0,0);
        LCD_Print(l1);

        LCD_SetCursor(1,0);
        LCD_Print(l2);
    }
}

/* Public API */
void Scanner_Init(void)
{
    APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== SCANNER INIT ===\r\n");
    
    // Initialize tracked nodes
    memset(trackedNodes, 0, sizeof(trackedNodes));
    
    // Initialize peripherals
    LCD_Init();
    Buzzer_Init();
    
    // Configure radio
    Radio_ConfigureRx();
    
    // Create timers
    UTIL_TIMER_Create(&lcdUpdateTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_PERIODIC,
                      OnLcdUpdateTimer, 
                      NULL);
    UTIL_TIMER_SetPeriod(&lcdUpdateTimer, LCD_UPDATE_INTERVAL_MS);
    UTIL_TIMER_Start(&lcdUpdateTimer);
    
    UTIL_TIMER_Create(&buzzerTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_PERIODIC,
                      OnBuzzerTimer, 
                      NULL);
    UTIL_TIMER_SetPeriod(&buzzerTimer, BUZZER_UPDATE_INTERVAL_MS);
    UTIL_TIMER_Start(&buzzerTimer);
    
    // Start receiving
    Radio.Rx(0);
    
    // Initial LCD display
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("PERSONAL NODE");
    LCD_SetCursor(1, 0);
    LCD_Print("SCANNER v1.0");
    LCD_SetCursor(2, 0);
    LCD_Print("Initializing...");
    HAL_Delay(2000);
    
    APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Ready\r\n");
}

void Scanner_Process(void)
{
    uint32_t now = HAL_GetTick();

    // =========================
    // 1. TIMEOUT NODES
    // =========================
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++)
    {
        if (trackedNodes[i].isActive)
        {
            if ((now - trackedNodes[i].lastSeenTime) > NODE_TIMEOUT_MS)
            {
                APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Node %d timeout\r\n",
                        trackedNodes[i].nodeId);

                trackedNodes[i].isActive = 0;
            }
        }
    }

    // =========================
    // 2. JOYSTICK READ
    // =========================
    Read_Joystick();

    static uint32_t lastMove = 0;

    if (now - lastMove < 200) return; // anti-rebond UI

    // get active nodes count
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;
    Scanner_GetAllActiveNodes(nodes, &count);

    // =========================
    // 3. LIST MODE (scroll)
    // =========================
    if (uiMode == 0)
    {
        // DOWN
        if (joy_y > 3000)
        {
            if (scrollIndex + 1 < count - 1)
                scrollIndex++;

            lastMove = now;
        }

        // UP
        if (joy_y < 1000)
        {
            if (scrollIndex > 0)
                scrollIndex--;

            lastMove = now;
        }

        // CLICK (joystick button)
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET)
        {
            selectedNodeIndex = scrollIndex;
            uiMode = 1; // switch to detail view
            lastMove = now;
        }
    }

    // =========================
    // 4. DETAIL MODE
    // =========================
    else
    {
        // CLICK = return list
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET)
        {
            uiMode = 0;
            lastMove = now;
        }
    }
}

TrackedNode_t* Scanner_GetNode(uint8_t nodeId)
{
    if (nodeId < 1 || nodeId > MAX_TRACKED_NODES) {
        return NULL;
    }
    return &trackedNodes[nodeId - 1];
}

void Scanner_GetAllActiveNodes(TrackedNode_t* nodes, uint8_t* count)
{
    *count = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++) {
        if (trackedNodes[i].isActive) {
            memcpy(&nodes[*count], &trackedNodes[i], sizeof(TrackedNode_t));
            (*count)++;
        }
    }
}

uint8_t Scanner_GetClosestNode(void)
{
    int16_t bestRssi = -200;
    uint8_t closestId = 0;
    
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++) {
        if (trackedNodes[i].isActive && trackedNodes[i].rssi > bestRssi) {
            bestRssi = trackedNodes[i].rssi;
            closestId = trackedNodes[i].nodeId;
        }
    }
    
    return closestId;
}

uint8_t Scanner_GetMostUrgentNode(void)
{
    // Priority: SOS > FALL > closest OK node with weak signal
    
    // Check for SOS first
    int16_t bestSosRssi = -200;
    uint8_t sosId = 0;
    
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++) {
        if (trackedNodes[i].isActive && 
            trackedNodes[i].status == NODE_STATUS_SOS &&
            trackedNodes[i].rssi > bestSosRssi) {
            bestSosRssi = trackedNodes[i].rssi;
            sosId = trackedNodes[i].nodeId;
        }
    }
    
    if (sosId > 0) return sosId;
    
    // Check for FALL
    int16_t bestFallRssi = -200;
    uint8_t fallId = 0;
    
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++) {
        if (trackedNodes[i].isActive && 
            trackedNodes[i].status == NODE_STATUS_FALL &&
            trackedNodes[i].rssi > bestFallRssi) {
            bestFallRssi = trackedNodes[i].rssi;
            fallId = trackedNodes[i].nodeId;
        }
    }
    
    return fallId;  // 0 if no urgent nodes
}

ProximityLevel_t Scanner_RssiToProximity(int16_t rssi)
{
    if (rssi > RSSI_VERY_CLOSE) {
        return PROXIMITY_VERY_CLOSE;
    } else if (rssi > RSSI_CLOSE) {
        return PROXIMITY_CLOSE;
    } else if (rssi > RSSI_MEDIUM) {
        return PROXIMITY_MEDIUM;
    } else if (rssi > RSSI_FAR) {
        return PROXIMITY_FAR;
    } else {
        return PROXIMITY_VERY_FAR;
    }
}

const char* Scanner_ProximityToString(ProximityLevel_t proximity)
{
    switch (proximity) {
        case PROXIMITY_VERY_CLOSE: return "V.CL";
        case PROXIMITY_CLOSE:      return "CLO";
        case PROXIMITY_MEDIUM:     return "MED";
        case PROXIMITY_FAR:        return "FAR";
        case PROXIMITY_VERY_FAR:   return "V.FAR";
        default:                   return "N.D";
    }
}

const char* Scanner_StatusToString(NodeStatus_t status)
{
    switch (status) {
        case NODE_STATUS_OK:   return "OK";
        case NODE_STATUS_FALL: return "FALL";
        case NODE_STATUS_SOS:  return "SOS";
        default:               return "???";
    }
}