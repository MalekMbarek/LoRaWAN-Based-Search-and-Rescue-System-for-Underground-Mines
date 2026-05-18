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
#include <math.h>
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
#define URGENT_HOLD_TIME_MS   30000 
/* Update intervals */
#define LCD_UPDATE_INTERVAL_MS          500
#define BUZZER_UPDATE_INTERVAL_MS       100
extern ADC_HandleTypeDef hadc;
/* Private variables */
static uint8_t uiMode;
//static void Read_Joystick(void);
static uint8_t uiMode = 0;
static uint8_t selectedNodeIndex = 0;
static uint8_t detailPage = 0;
static uint32_t lastPageChange = 0;
static RadioEvents_t radioEvents;
static TrackedNode_t trackedNodes[MAX_TRACKED_NODES];
static UTIL_TIMER_Object_t lcdUpdateTimer;
static UTIL_TIMER_Object_t buzzerTimer;
static uint8_t buzzerActive = 0;
static uint32_t nextBuzzerTime = 0;
static uint8_t scrollIndex = 0;
static UI_State_t uiState = UI_LIST_VIEW;
static float RSSI_To_Distance(int16_t rssi);
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
    if (packet->nodeId == 0 || packet->nodeId > MAX_TRACKED_NODES) {
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
    if (packet->spo2 == 0 || packet->spo2 > 100)
{
    trackedNodes[index].spo2 = 0;
}
else
{
    trackedNodes[index].spo2 = packet->spo2;
} 
    trackedNodes[index].accX = packet->accX;
    trackedNodes[index].accY = packet->accY;
    trackedNodes[index].accZ = packet->accZ;
    trackedNodes[index].rssi = rssi;
    trackedNodes[index].snr = snr;
    trackedNodes[index].proximity = Scanner_RssiToProximity(rssi);
    trackedNodes[index].lastSeenTime = HAL_GetTick();

    trackedNodes[index].isActive = 1;
    trackedNodes[index].packetCount++;
   if (packet->status == NODE_STATUS_SOS ||
    packet->status == NODE_STATUS_FALL)
{
    trackedNodes[index].isUrgent = 1;
    trackedNodes[index].urgentStartTime = HAL_GetTick();
}
    #define ALPHA 0.6f

trackedNodes[index].rssi =
    (int16_t)(ALPHA * rssi + (1 - ALPHA) * trackedNodes[index].rssi);
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
static int compareNodes(const void *a, const void *b)
{
    TrackedNode_t *n1 = (TrackedNode_t *)a;
    TrackedNode_t *n2 = (TrackedNode_t *)b;

    int p1 = (n1->isUrgent) ? 3 :
             (n1->status == NODE_STATUS_SOS) ? 3 :
             (n1->status == NODE_STATUS_FALL) ? 2 : 1;

    int p2 = (n2->isUrgent) ? 3 :
             (n2->status == NODE_STATUS_SOS) ? 3 :
             (n2->status == NODE_STATUS_FALL) ? 2 : 1;

    if (p1 != p2)
        return p2 - p1;

    return n2->rssi - n1->rssi;
}
/* LCD display update */
static void UpdateLcdDisplay(void)
{
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;
    Scanner_GetAllActiveNodes(nodes, &count);

    // Sort nodes: urgent (SOS/FALL) first, then by RSSI
    qsort(nodes, count, sizeof(TrackedNode_t), compareNodes);

    // Static buffers for current and previous line content (20 columns max)
    static char prevLine0[LCD_COLS + 1] = "";
    static char prevLine1[LCD_COLS + 1] = "";
    char line0[LCD_COLS + 1] = {0};
    char line1[LCD_COLS + 1] = {0};

    // ================= LIST MODE =================
    if (uiMode == 0)
    {
        if (count == 0)
        {
            strcpy(line0, "SCANNING...");
            strcpy(line1, "No nodes");
        }
        else
        {
            // Build line0 (first visible row)
            uint8_t idx0 = scrollIndex;
            if (idx0 < count)
            {
                float dist = RSSI_To_Distance(nodes[idx0].rssi);
                int d_int = (int)dist;
                int d_dec = (int)((dist - d_int) * 10);

                // Prefix '>' if this is the selected node
                char prefix = (idx0 == selectedNodeIndex) ? '>' : ' ';
                snprintf(line0, LCD_COLS, "%cN%d %s %d.%dm",
                         prefix,
                         nodes[idx0].nodeId,
                         Scanner_StatusToString(nodes[idx0].status),
                         d_int, d_dec);
            }
            else
            {
                line0[0] = '\0';
            }

            // Build line1 (second visible row)
            uint8_t idx1 = scrollIndex + 1;
            if (idx1 < count)
            {
                float dist = RSSI_To_Distance(nodes[idx1].rssi);
                int d_int = (int)dist;
                int d_dec = (int)((dist - d_int) * 10);

                char prefix = (idx1 == selectedNodeIndex) ? '>' : ' ';
                snprintf(line1, LCD_COLS, "%cN%d %s %d.%dm",
                         prefix,
                         nodes[idx1].nodeId,
                         Scanner_StatusToString(nodes[idx1].status),
                         d_int, d_dec);
            }
            else
            {
                line1[0] = '\0';
            }
        }

        // Only write to LCD if content changed (reduces flicker)
        if (strcmp(line0, prevLine0) != 0)
        {
            LCD_SetCursor(0, 0);
            LCD_Print(line0);
            strcpy(prevLine0, line0);
        }
        if (strcmp(line1, prevLine1) != 0)
        {
            LCD_SetCursor(1, 0);
            LCD_Print(line1);
            // Pad with spaces to erase leftover characters from previous longer line
            for (int i = strlen(line1); i < LCD_COLS; i++)
                LCD_PrintChar(' ');
            strcpy(prevLine1, line1);
        }
    }
    // ================= DETAIL MODE =================
    else
    {
        uint8_t id = selectedNodeIndex;
        // Safety: ensure selectedNodeIndex is within active nodes
        if (id >= count)
        {
            // Fallback: select first node
            if (count > 0) id = 0;
            else id = 0; // no nodes, will show empty
        }

        char l1[LCD_COLS + 1];
        char l2[LCD_COLS + 1];

        if (count == 0)
        {
            strcpy(l1, "No nodes");
            strcpy(l2, "");
        }
        else
        {
            TrackedNode_t* n = &nodes[id];
            float dist = RSSI_To_Distance(n->rssi);
            int d_int = (int)dist;
            int d_dec = (int)((dist - d_int) * 10);

            snprintf(l1, LCD_COLS, "N%d %s",
                     n->nodeId,
                     Scanner_StatusToString(n->status));

            if (detailPage == 0)
                snprintf(l2, LCD_COLS, "D:%d.%dm", d_int, d_dec);
            else
                snprintf(l2, LCD_COLS, "HR:%d SpO2:%d", n->heartRate, n->spo2);
        }

        // Update line0 if changed
        if (strcmp(l1, prevLine0) != 0)
        {
            LCD_SetCursor(0, 0);
            LCD_Print(l1);
            // Erase rest of line
            for (int i = strlen(l1); i < LCD_COLS; i++)
                LCD_PrintChar(' ');
            strcpy(prevLine0, l1);
        }
        // Update line1 if changed
        if (strcmp(l2, prevLine1) != 0)
        {
            LCD_SetCursor(1, 0);
            LCD_Print(l2);
            for (int i = strlen(l2); i < LCD_COLS; i++)
                LCD_PrintChar(' ');
            strcpy(prevLine1, l2);
        }
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
    LCD_Print("SCANNER NODE");

    LCD_SetCursor(2, 0);
    LCD_Print("Initializing...");
    HAL_Delay(2000);
    LCD_Clear();

    APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Ready\r\n");
}

void Scanner_Process(void)
{
    uint32_t now = HAL_GetTick();

    // Node timeout and urgency management (unchanged)
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++)
    {
        if (trackedNodes[i].isActive)
        {
            uint32_t timeout = NODE_TIMEOUT_MS;
            if (trackedNodes[i].isUrgent)
            {
                if (now - trackedNodes[i].urgentStartTime < URGENT_HOLD_TIME_MS)
                    timeout = NODE_TIMEOUT_MS + URGENT_HOLD_TIME_MS;
                else
                    trackedNodes[i].isUrgent = 0;
            }
            if ((now - trackedNodes[i].lastSeenTime) > timeout)
                trackedNodes[i].isActive = 0;
        }
    }

    // Joystick reading
    Read_Joystick();

    static uint32_t lastMove = 0;
    if (now - lastMove < 200) return; // debounce

    // Get active nodes
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;
    Scanner_GetAllActiveNodes(nodes, &count);

    // ================= LIST MODE =================
    if (uiMode == 0)
    {
        // DOWN
        if (joy_y > 3000)
        {
            if (scrollIndex + 1 < count)
                scrollIndex++;
            // ⭐ Update selectedNodeIndex to follow scroll
            selectedNodeIndex = scrollIndex;
            lastMove = now;
        }
        // UP
        if (joy_y < 1000)
        {
            if (scrollIndex > 0)
                scrollIndex--;
            // ⭐ Update selectedNodeIndex to follow scroll
            selectedNodeIndex = scrollIndex;
            lastMove = now;
        }
        // CLICK – enter detail view
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET)
        {
            // selectedNodeIndex already equals scrollIndex
            uiMode = 1;
            lastMove = now;
        }
    }
    // ================= DETAIL MODE =================
    else
    {
        if (joy_x > 3000)   // right
        {
            detailPage = (detailPage + 1) % 2;
            lastMove = now;
        }
        if (joy_x < 1000)   // left
        {
            detailPage = (detailPage == 0) ? 1 : 0;
            lastMove = now;
        }
        // CLICK – return to list
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
static float RSSI_To_Distance(int16_t rssi)
{
    // Calibration (à mesurer chez toi idéalement)
    float A = -50.0f;   // RSSI à 1m
    float n = 2.3f;     // environnement (2.0 open, 3.0 indoor chargé)

    float ratio = (A - rssi) / (10.0f * n);

    float distance = powf(10.0f, ratio);

    // limitation réaliste
    if (distance < 0.2f) distance = 0.2f;
    if (distance > 25.0f) distance = 25.0f;

    return distance;
}
