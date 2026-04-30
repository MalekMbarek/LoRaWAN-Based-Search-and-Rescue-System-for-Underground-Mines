/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scanner_app.h
  * @brief   Scanner application for personal node detection
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SCANNER_APP_H__
#define __SCANNER_APP_H__

#include <stdint.h>

/* Maximum number of personal nodes to track */
#define MAX_TRACKED_NODES       4

/* RSSI thresholds for proximity indication */
#define RSSI_VERY_CLOSE         -40   // < 2 meters
#define RSSI_CLOSE              -60   // 2-5 meters
#define RSSI_MEDIUM             -80   // 5-15 meters
#define RSSI_FAR                -100  // 15-50 meters
// Anything below -100 is VERY_FAR

/* Buzzer pattern timing (ms) */
#define BUZZ_VERY_CLOSE_PERIOD  100
#define BUZZ_CLOSE_PERIOD       300
#define BUZZ_MEDIUM_PERIOD      800
#define BUZZ_FAR_PERIOD         2000

/* Node status from personal node */
typedef enum {
    NODE_STATUS_OK = 0,
    NODE_STATUS_FALL = 1,
    NODE_STATUS_SOS = 2
} NodeStatus_t;

/* Proximity level based on RSSI */
typedef enum {
    PROXIMITY_VERY_FAR = 0,
    PROXIMITY_FAR = 1,
    PROXIMITY_MEDIUM = 2,
    PROXIMITY_CLOSE = 3,
    PROXIMITY_VERY_CLOSE = 4
} ProximityLevel_t;

/* Personal node packet structure (10 bytes) */
typedef struct {
    uint8_t pktType;      // PKT_TYPE_FULL
    uint8_t nodeId;       // 1-4
    uint8_t status;       // NodeStatus_t
    uint8_t heartRate;    // BPM or 0/255
    int16_t accX;         // mg
    int16_t accY;         // mg
    int16_t accZ;         // mg
} __attribute__((packed)) PersonalNodePacket_t;

/* Tracked node information */
typedef struct {
    uint8_t nodeId;
    NodeStatus_t status;
    uint8_t heartRate;
    int16_t accX;
    int16_t accY;
    int16_t accZ;
    int16_t rssi;
    int8_t snr;
    ProximityLevel_t proximity;
    uint32_t lastSeenTime;
    uint8_t isActive;
    uint8_t packetCount;
} TrackedNode_t;
typedef enum {
    UI_LIST_VIEW = 0,
    UI_DETAIL_VIEW = 1
} UI_State_t;
/* Function prototypes */
void Scanner_Init(void);
void Scanner_Process(void);
TrackedNode_t* Scanner_GetNode(uint8_t nodeId);
void Scanner_GetAllActiveNodes(TrackedNode_t* nodes, uint8_t* count);
uint8_t Scanner_GetClosestNode(void);
uint8_t Scanner_GetMostUrgentNode(void);
ProximityLevel_t Scanner_RssiToProximity(int16_t rssi);
const char* Scanner_ProximityToString(ProximityLevel_t proximity);
const char* Scanner_StatusToString(NodeStatus_t status);

#endif /* __SCANNER_APP_H__ */