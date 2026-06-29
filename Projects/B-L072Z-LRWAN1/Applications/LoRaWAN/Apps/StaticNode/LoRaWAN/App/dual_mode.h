/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dual_mode.h
  * @brief   Dual mode LoRaWAN/Raw LoRa management for static node
  *
  * @note    Handles mode switching between LoRaWAN and Raw LoRa RX,
  *          stores and manages data from personal nodes
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DUAL_MODE_H__
#define __DUAL_MODE_H__

#include <stdint.h>

/* ============================================================
 * Constants
 * ============================================================ */
#define MAX_PERSONAL_NODES      4     /* Maximum number of personal nodes to track */

/* ============================================================
 * Enumerations
 * ============================================================ */

/**
  * @brief Operating modes for dual mode operation
  */
typedef enum {
    MODE_LORAWAN = 0,        /* Normal LoRaWAN operation */
    MODE_RAW_LORA_RX = 1     /* Raw LoRa receive mode for personal nodes */
} OperatingMode_t;

/**
  * @brief Personal node status codes
  */
typedef enum {
    STATUS_OK = 0,           /* Normal operation */
    STATUS_FALL = 1,         /* Fall detected */
    STATUS_SOS = 2           /* SOS button pressed */
} PersonalNodeStatus_t;

/* ============================================================
 * Data Structures
 * ============================================================ */

/**
  * @brief Personal node packet structure
  * @note  Matches personal node packet format (PKT_TYPE_FULL)
  */
typedef struct {
    uint8_t pktType;         /* Packet type (PKT_TYPE_FULL) */
    uint8_t nodeId;          /* Node ID (1-4) */
    uint8_t status;          /* STATUS_OK, STATUS_FALL, STATUS_SOS */
    uint8_t heartRate;       /* Heart rate in BPM (0 = invalid) */
    uint8_t spo2;            /* SpO2 percentage (0 = invalid) */
    int16_t accX;            /* X-axis acceleration (mg) */
    int16_t accY;            /* Y-axis acceleration (mg) */
    int16_t accZ;            /* Z-axis acceleration (mg) */
    int16_t rssi;            /* Received signal strength (dBm) */
    float distance;          /* Estimated distance (meters) */
} PersonalNodePacket_t;

/**
  * @brief Stored personal node data with metadata
  */
typedef struct {
    uint8_t nodeId;           /* Node ID (1-4) */
    uint8_t status;           /* Last known status */
    uint8_t heartRate;        /* Last heart rate (BPM) */
    uint8_t spo2;             /* Last SpO2 (%) */
    int16_t accX;             /* Last X acceleration (mg) */
    int16_t accY;             /* Last Y acceleration (mg) */
    int16_t accZ;             /* Last Z acceleration (mg) */
    int16_t rssi;             /* RSSI of last packet (dBm) */
    float distance;           /* Estimated distance (meters) */
    uint32_t lastUpdateTime;  /* HAL_GetTick() timestamp of last update */
    uint8_t isActive;         /* 1 if node has been seen, 0 otherwise */
    uint8_t hasAlert;         /* 1 if FALL or SOS status active */
} StoredPersonalNode_t;

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/* Initialization and mode control */
void DualMode_Init(void);
void DualMode_Switch(OperatingMode_t newMode);
OperatingMode_t DualMode_GetCurrent(void);
void DualMode_Process(void);

/* LoRaWAN state management */
uint8_t DualMode_IsLoRaWANSuspended(void);
uint8_t DualMode_IsLoRaWANJoined(void);
void DualMode_SetJoinStatus(uint8_t joined);

/* Alert management */
void DualMode_SetAlertPending(uint8_t pending);
uint8_t DualMode_IsAlertPending(void);

/* Personal node data access */
PersonalNodePacket_t* DualMode_GetLastPersonalNodeData(void);
StoredPersonalNode_t* DualMode_GetStoredNode(uint8_t nodeId);
uint8_t DualMode_GetActiveNodeCount(void);
void DualMode_GetAllActiveNodes(StoredPersonalNode_t* nodes, uint8_t* count);
uint8_t DualMode_HasAnyAlerts(void);
void DualMode_ClearAlert(uint8_t nodeId);

#endif /* __DUAL_MODE_H__ */