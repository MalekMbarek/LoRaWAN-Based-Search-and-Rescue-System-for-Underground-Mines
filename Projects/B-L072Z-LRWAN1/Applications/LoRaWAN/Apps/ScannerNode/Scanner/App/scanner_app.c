/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scanner_app.c
  * @brief   Scanner application implementation for ScannerNode
  *          - Raw LoRa receiver for personal node tracking
  *          - LCD display with list and detail views
  *          - Buzzer alerts based on node proximity and status
  *          - Joystick navigation and selection
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

/* ============================================================
 * Raw LoRa Configuration (must match personal nodes)
 * ============================================================ */
#define RAW_RF_FREQUENCY                868000000
#define RAW_LORA_BANDWIDTH              0          /* 125 kHz */
#define RAW_LORA_SPREADING_FACTOR       7
#define RAW_LORA_CODINGRATE             1
#define RAW_LORA_PREAMBLE_LENGTH        8
#define RAW_LORA_SYMBOL_TIMEOUT         5
#define RAW_LORA_FIX_LENGTH_PAYLOAD_ON  false
#define RAW_LORA_IQ_INVERSION_ON        false

/* ============================================================
 * Timing Constants
 * ============================================================ */
#define NODE_TIMEOUT_MS                 60000     /* Node considered inactive after 60s */
#define URGENT_HOLD_TIME_MS             40000     /* Hold urgent status for 40s */
#define LCD_UPDATE_INTERVAL_MS          500       /* LCD refresh rate */
#define BUZZER_UPDATE_INTERVAL_MS       100       /* Buzzer update rate */

/* ============================================================
 * Joystick Thresholds
 * ============================================================ */
#define JOY_HIGH        3000
#define JOY_LOW         1000
#define JOY_CENTER_LOW  1600
#define JOY_CENTER_HIGH 2400

/* ============================================================
 * Private Variables
 * ============================================================ */
static uint8_t uiMode = 0;
static uint8_t selectedNodeIndex = 0;
static uint8_t detailPage = 0;
static RadioEvents_t radioEvents;
static TrackedNode_t trackedNodes[MAX_TRACKED_NODES];
static UTIL_TIMER_Object_t lcdUpdateTimer;
static UTIL_TIMER_Object_t buzzerTimer;
static uint8_t buzzerActive = 0;
static uint32_t nextBuzzerTime = 0;
static uint8_t scrollIndex = 0;
static float RSSI_To_Distance(int16_t rssi);

extern ADC_HandleTypeDef hadc;

/* ============================================================
 * Forward Declarations
 * ============================================================ */
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
static int compareNodes(const void *a, const void *b);

/* ============================================================
 * Radio Callbacks
 * ============================================================ */

/**
  * @brief  Raw LoRa RX done callback
  * @param  payload Received data buffer
  * @param  size Payload size in bytes
  * @param  rssi RSSI of received packet
  * @param  snr SNR of received packet
  */
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    APP_LOG(TS_OFF, VLEVEL_L, "[RX] Packet received: size=%d RSSI=%d SNR=%d\r\n", 
            size, rssi, snr);
    
    if (size >= PKT_SIZE) {
        PersonalNodePacket_t packet;
        
        /* Parse packet (matches personal node format) */
        packet.nodeId = payload[1];        /* PKT_I_ID */
        packet.status = payload[2];        /* PKT_I_STATUS */
        packet.heartRate = payload[3];     /* PKT_I_BPM */
        
        /* Parse accelerometer (big-endian) */
        packet.accX = (int16_t)((payload[4] << 8) | payload[5]);  /* PKT_I_AX_H/L */
        packet.accY = (int16_t)((payload[6] << 8) | payload[7]);  /* PKT_I_AY_H/L */
        packet.accZ = (int16_t)((payload[8] << 8) | payload[9]);  /* PKT_I_AZ_H/L */
        packet.spo2 = payload[10];  /* PKT_I_SPO2 */
        
        ProcessReceivedPacket(&packet, rssi, snr);
    } else {
        APP_LOG(TS_OFF, VLEVEL_M, "[RX] Packet too small: %d bytes\r\n", size);
    }
    
    /* Continue receiving */
    Radio.Rx(0);
}

static void OnRxTimeout(void)
{
    Radio.Rx(0);
}

static void OnRxError(void)
{
    APP_LOG(TS_OFF, VLEVEL_M, "[RX] Error\r\n");
    Radio.Rx(0);
}

static void OnTxDone(void)
{
    Radio.Sleep();
}

static void OnTxTimeout(void)
{
    Radio.Sleep();
}

/* ============================================================
 * Packet Processing
 * ============================================================ */

/**
  * @brief  Process received personal node packet
  * @param  packet Parsed packet data
  * @param  rssi Received signal strength
  * @param  snr Signal-to-noise ratio
  */
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

/**
  * @brief  Update tracked node with new packet data
  * @param  packet Parsed packet data
  * @param  rssi Received signal strength
  * @param  snr Signal-to-noise ratio
  */
static void UpdateTrackedNode(PersonalNodePacket_t* packet, int16_t rssi, int8_t snr)
{
    uint8_t index = packet->nodeId - 1;

    trackedNodes[index].nodeId    = packet->nodeId;
    trackedNodes[index].status    = (NodeStatus_t)packet->status;
    trackedNodes[index].heartRate = packet->heartRate;
    trackedNodes[index].spo2      = packet->spo2;
    trackedNodes[index].accX      = packet->accX;
    trackedNodes[index].accY      = packet->accY;
    trackedNodes[index].accZ      = packet->accZ;
    trackedNodes[index].snr       = snr;
    trackedNodes[index].proximity = Scanner_RssiToProximity(rssi);
    trackedNodes[index].lastSeenTime = HAL_GetTick();
    trackedNodes[index].isActive  = 1;
    trackedNodes[index].packetCount++;

    /* Smoothed RSSI using exponential moving average */
    #define ALPHA 0.6f
    trackedNodes[index].rssi =
        (int16_t)(ALPHA * rssi + (1.0f - ALPHA) * trackedNodes[index].rssi);

    /* Urgent flag for SOS or FALL status */
    if (packet->status == NODE_STATUS_SOS ||
        packet->status == NODE_STATUS_FALL)
    {
        trackedNodes[index].isUrgent      = 1;
        trackedNodes[index].urgentStartTime = HAL_GetTick();
        trackedNodes[index].urgentStatus  = packet->status;
    }
}

/* ============================================================
 * Radio Configuration
 * ============================================================ */

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
                      true,  /* CRC enabled */
                      0,
                      0,
                      RAW_LORA_IQ_INVERSION_ON,
                      true); /* Continuous RX */
    
    APP_LOG(TS_OFF, VLEVEL_M, "[Scanner] Radio configured\r\n");
}

/* ============================================================
 * Timer Callbacks
 * ============================================================ */

static void OnLcdUpdateTimer(void *context)
{
    UpdateLcdDisplay();
}

static void OnBuzzerTimer(void *context)
{
    UpdateBuzzer();
}

/* ============================================================
 * Buzzer Control
 * ============================================================ */

/**
  * @brief  Update buzzer based on most urgent node
  * @note   Buzzer pattern changes based on node proximity
  */
static void UpdateBuzzer(void)
{
    uint32_t now = HAL_GetTick();
    
    uint8_t urgentNodeId = Scanner_GetMostUrgentNode();
    
    if (urgentNodeId == 0) {
        Buzzer_Off();
        buzzerActive = 0;
        return;
    }
    
    TrackedNode_t* node = Scanner_GetNode(urgentNodeId);
    if (!node || !node->isActive) {
        Buzzer_Off();
        buzzerActive = 0;
        return;
    }
    
    /* Determine buzzer period based on proximity */
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
    
    /* Toggle buzzer */
    if (now >= nextBuzzerTime) {
        if (buzzerActive) {
            Buzzer_Off();
            buzzerActive = 0;
            nextBuzzerTime = now + period;
        } else {
            Buzzer_Beep(50);
            buzzerActive = 1;
            nextBuzzerTime = now + 50;
        }
    }
}

/* ============================================================
 * LCD Display Functions
 * ============================================================ */

/**
  * @brief  Compare nodes for sorting (priority: urgent > SOS > FALL > RSSI)
  */
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

/**
  * @brief  Get display string for node status
  * @param  n Pointer to tracked node
  * @return Status string (OK, FALL, SOS)
  */
static const char* DisplayStatus(const TrackedNode_t* n)
{
    if (n->isUrgent)
    {
        uint32_t elapsed = HAL_GetTick() - n->urgentStartTime;
        if (elapsed < URGENT_HOLD_TIME_MS)
        {
            if (n->status == NODE_STATUS_SOS)  return "SOS";
            if (n->status == NODE_STATUS_FALL) return "FALL";
            return Scanner_StatusToString((NodeStatus_t)n->urgentStatus);
        }
    }
    return Scanner_StatusToString(n->status);
}

/**
  * @brief  Print padded string to LCD row
  * @param  row Row number (0-3)
  * @param  str String to display (padded with spaces)
  */
static void LCD_PrintPadded(uint8_t row, const char* str)
{
    LCD_SetCursor(row, 0);
    uint8_t len = 0;
    while (str[len] && len < LCD_COLS)
    {
        LCD_PrintChar(str[len]);
        len++;
    }
    /* Pad remainder with spaces to erase leftover characters */
    while (len < LCD_COLS)
    {
        LCD_PrintChar(' ');
        len++;
    }
}

/**
  * @brief  Update LCD display
  * @note   Shows list view or detail view based on UI mode
  */
static void UpdateLcdDisplay(void)
{
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;
    Scanner_GetAllActiveNodes(nodes, &count);
    qsort(nodes, count, sizeof(TrackedNode_t), compareNodes);

    char line0[LCD_COLS + 1] = {0};
    char line1[LCD_COLS + 1] = {0};

    /* List View */
    if (uiMode == 0)
    {
        if (count == 0)
        {
            strncpy(line0, "SCANNING...",        LCD_COLS);
            strncpy(line1, "No nodes",           LCD_COLS);
        }
        else
        {
            /* Row 0 */
            if (scrollIndex < count)
            {
                float dist  = RSSI_To_Distance(nodes[scrollIndex].rssi);
                int d_int   = (int)dist;
                int d_dec   = (int)((dist - d_int) * 10);
                char prefix = (scrollIndex == selectedNodeIndex) ? '>' : ' ';
                snprintf(line0, LCD_COLS + 1, "%cN%d %-4s %d.%dm",
                         prefix,
                         nodes[scrollIndex].nodeId,
                         DisplayStatus(&nodes[scrollIndex]),
                         d_int, d_dec);
            }

            /* Row 1 */
            uint8_t idx1 = scrollIndex + 1;
            if (idx1 < count)
            {
                float dist  = RSSI_To_Distance(nodes[idx1].rssi);
                int d_int   = (int)dist;
                int d_dec   = (int)((dist - d_int) * 10);
                char prefix = (idx1 == selectedNodeIndex) ? '>' : ' ';
                snprintf(line1, LCD_COLS + 1, "%cN%d %-4s %d.%dm",
                         prefix,
                         nodes[idx1].nodeId,
                         DisplayStatus(&nodes[idx1]),
                         d_int, d_dec);
            }
        }

        LCD_PrintPadded(0, line0);
        LCD_PrintPadded(1, line1);
    }
    /* Detail View */
    else
    {
        uint8_t id = (count > 0 && selectedNodeIndex < count)
                     ? selectedNodeIndex : 0;

        if (count == 0)
        {
            strncpy(line0, "No nodes", LCD_COLS);
            line1[0] = '\0';
        }
        else
        {
            TrackedNode_t* n = &nodes[id];
            float dist = RSSI_To_Distance(n->rssi);
            int d_int  = (int)dist;
            int d_dec  = (int)((dist - d_int) * 10);

            /* Line 0: Node ID and status */
            snprintf(line0, LCD_COLS + 1, "N%d %-4s",
                     n->nodeId, DisplayStatus(n));

            /* Line 1: Page 0 = distance, Page 1 = vitals */
            if (detailPage == 0)
                snprintf(line1, LCD_COLS + 1, "D:%d.%dm", d_int, d_dec);
            else
                snprintf(line1, LCD_COLS + 1, "HR:%d SpO2:%d%%",
                         n->heartRate, n->spo2);
        }

        LCD_PrintPadded(0, line0);
        LCD_PrintPadded(1, line1);
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

void Scanner_Init(void)
{
    APP_LOG(TS_OFF, VLEVEL_M, "\r\n=== SCANNER INIT ===\r\n");
    
    memset(trackedNodes, 0, sizeof(trackedNodes));
    
    LCD_Init();
    Buzzer_Init();
    
    Radio_ConfigureRx();
    
    /* Create LCD update timer */
    UTIL_TIMER_Create(&lcdUpdateTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_PERIODIC,
                      OnLcdUpdateTimer, 
                      NULL);
    UTIL_TIMER_SetPeriod(&lcdUpdateTimer, LCD_UPDATE_INTERVAL_MS);
    UTIL_TIMER_Start(&lcdUpdateTimer);
    
    /* Create buzzer update timer */
    UTIL_TIMER_Create(&buzzerTimer, 
                      0xFFFFFFFFU, 
                      UTIL_TIMER_PERIODIC,
                      OnBuzzerTimer, 
                      NULL);
    UTIL_TIMER_SetPeriod(&buzzerTimer, BUZZER_UPDATE_INTERVAL_MS);
    UTIL_TIMER_Start(&buzzerTimer);
    
    /* Start receiving */
    Radio.Rx(0);
    
    /* Initial LCD display */
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

    /* Node timeout and urgency expiry */
    for (uint8_t i = 0; i < MAX_TRACKED_NODES; i++)
    {
        if (!trackedNodes[i].isActive) continue;

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

    /* Read joystick ADC values */
    Read_Joystick();

    /* Joystick direction detection */
    typedef enum { DIR_NONE, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } JoyDir_t;
    JoyDir_t dir = DIR_NONE;

    if      (joy_y > JOY_HIGH) dir = DIR_DOWN;
    else if (joy_y < JOY_LOW)  dir = DIR_UP;
    else if (joy_x > JOY_HIGH) dir = DIR_RIGHT;
    else if (joy_x < JOY_LOW)  dir = DIR_LEFT;

    /* Stick latch - prevents repeated triggers */
    static uint8_t stickLatch = 0;

    uint8_t stickCentered = (joy_x > JOY_CENTER_LOW && joy_x < JOY_CENTER_HIGH &&
                             joy_y > JOY_CENTER_LOW && joy_y < JOY_CENTER_HIGH);

    if (stickCentered) stickLatch = 0;

    uint8_t stickFired = 0;
    if (dir != DIR_NONE && !stickLatch)
    {
        stickLatch = 1;
        stickFired = 1;
    }

    /* Button edge detection */
    uint8_t btnPressed         = Joystick_SW_Pressed();
    static uint8_t prevBtn     = 0;
    uint8_t btnEdge            = (btnPressed && !prevBtn);
    prevBtn                    = btnPressed;

    if (!stickFired && !btnEdge) return;

    /* Get active nodes */
    TrackedNode_t nodes[MAX_TRACKED_NODES];
    uint8_t count = 0;
    Scanner_GetAllActiveNodes(nodes, &count);

    /* List View navigation */
    if (uiMode == 0)
    {
        if (stickFired && dir == DIR_DOWN)
        {
            if (count > 0 && scrollIndex + 1 < count)
            {
                scrollIndex++;
                selectedNodeIndex = scrollIndex;
            }
        }
        else if (stickFired && dir == DIR_UP)
        {
            if (scrollIndex > 0)
            {
                scrollIndex--;
                selectedNodeIndex = scrollIndex;
            }
        }
        else if (btnEdge)
        {
            if (count > 0)
            {
                selectedNodeIndex = scrollIndex;
                detailPage = 0;
                uiMode = 1;
            }
        }
    }
    /* Detail View navigation */
    else
    {
        if (stickFired && dir == DIR_RIGHT)
        {
            detailPage = (detailPage + 1) % 2;
        }
        else if (stickFired && dir == DIR_LEFT)
        {
            detailPage = (detailPage == 0) ? 1 : 0;
        }
        else if (btnEdge)
        {
            uiMode = 0;
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
    /* Priority: SOS > FALL > closest OK node */
    
    /* Check for SOS first */
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
    
    /* Check for FALL */
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
    
    return fallId;
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

/* ============================================================
 * Private Helper Functions
 * ============================================================ */

/**
  * @brief  Convert RSSI to distance using log-distance model
  * @param  rssi Received signal strength in dBm
  * @return Estimated distance in meters
  * @note   A = RSSI at 1m, n = path loss exponent
  */
static float RSSI_To_Distance(int16_t rssi)
{
    float A = -50.0f;   /* RSSI at 1m (calibrate for your environment) */
    float n = 2.3f;     /* Path loss exponent (2.0 free space, 3.0 indoor) */

    float ratio = (A - rssi) / (10.0f * n);
    float distance = powf(10.0f, ratio);

    /* Clamp to realistic values */
    if (distance < 0.2f) distance = 0.2f;
    if (distance > 25.0f) distance = 25.0f;

    return distance;
}