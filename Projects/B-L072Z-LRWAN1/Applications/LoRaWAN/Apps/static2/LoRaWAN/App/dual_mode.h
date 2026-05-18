/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dual_mode.h
  * @brief   Dual mode LoRaWAN/Raw LoRa management
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DUAL_MODE_H__
#define __DUAL_MODE_H__

#include <stdint.h>

/* Maximum number of personal nodes to track */
#define MAX_PERSONAL_NODES      4

/* Operating modes */
typedef enum {
    MODE_LORAWAN = 0,
    MODE_RAW_LORA_RX = 1
} OperatingMode_t;

/* Personal node status */
typedef enum {
    STATUS_OK = 0,
    STATUS_FALL = 1,
    STATUS_SOS = 2
} PersonalNodeStatus_t;

/* Personal node packet structure (match your document 3) */
typedef struct {
    uint8_t pktType;      // PKT_TYPE_FULL
    uint8_t nodeId;       // MY_NODE_ID
    uint8_t status;       // STATUS_OK, STATUS_FALL, STATUS_SOS
    uint8_t heartRate; 
    uint8_t spo2;   // BPM or 0/255
    int16_t accX;
    int16_t accY;
    int16_t accZ;
      int16_t rssi;
float distance;
} PersonalNodePacket_t;

/* Stored personal node data with metadata */
typedef struct {
    uint8_t nodeId;           // Node ID (1-4)
    uint8_t status;           // Last known status
    uint8_t heartRate;        // Last heart rate
     uint8_t spo2;
    int16_t accX;             // Last X acceleration
    int16_t accY;             // Last Y acceleration
    int16_t accZ;             // Last Z acceleration
    uint32_t lastUpdateTime;  // HAL_GetTick() timestamp of last update
    uint8_t isActive;         // 1 if node has been seen, 0 otherwise
    uint8_t hasAlert;         // 1 if FALL or SOS status
    int16_t rssi;
 float distance;
} StoredPersonalNode_t;

/* Function prototypes */
void DualMode_Init(void);
void DualMode_Switch(OperatingMode_t newMode);
OperatingMode_t DualMode_GetCurrent(void);
uint8_t DualMode_IsLoRaWANSuspended(void);
uint8_t DualMode_IsLoRaWANJoined(void);
void DualMode_SetJoinStatus(uint8_t joined);
void DualMode_Process(void);

/* Personal node data access functions */
PersonalNodePacket_t* DualMode_GetLastPersonalNodeData(void);
StoredPersonalNode_t* DualMode_GetStoredNode(uint8_t nodeId);
uint8_t DualMode_GetActiveNodeCount(void);
void DualMode_GetAllActiveNodes(StoredPersonalNode_t* nodes, uint8_t* count);
uint8_t DualMode_HasAnyAlerts(void);
void DualMode_ClearAlert(uint8_t nodeId);
void DualMode_SetAlertPending(uint8_t pending);
uint8_t DualMode_IsAlertPending(void);
#endif /* __DUAL_MODE_H__ */