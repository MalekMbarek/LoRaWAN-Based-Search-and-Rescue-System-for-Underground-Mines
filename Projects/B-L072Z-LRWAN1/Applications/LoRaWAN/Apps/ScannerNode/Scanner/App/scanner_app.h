/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scanner_app.h
  * @brief   Scanner application for personal node detection
  *          - Raw LoRa receiver for tracking personal nodes
  *          - Proximity estimation via RSSI
  *          - Node status monitoring (OK, FALL, SOS)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SCANNER_APP_H__
#define __SCANNER_APP_H__

#include <stdint.h>

/* ============================================================
 * Constants
 * ============================================================ */

/* Maximum number of personal nodes to track */
#define MAX_TRACKED_NODES       4

/* RSSI thresholds for proximity indication */
#define RSSI_VERY_CLOSE         -40   /* < 2 meters */
#define RSSI_CLOSE              -60   /* 2-5 meters */
#define RSSI_MEDIUM             -80   /* 5-15 meters */
#define RSSI_FAR                -100  /* 15-50 meters */
                                      /* Below -100 = VERY_FAR */

/* Buzzer pattern timing (ms) */
#define BUZZ_VERY_CLOSE_PERIOD  100
#define BUZZ_CLOSE_PERIOD       300
#define BUZZ_MEDIUM_PERIOD      800
#define BUZZ_FAR_PERIOD         2000

/* Packet field indices */
#define PKT_I_TYPE    0
#define PKT_I_ID      1
#define PKT_I_STATUS  2
#define PKT_I_BPM     3
#define PKT_I_AX_H    4
#define PKT_I_AX_L    5
#define PKT_I_AY_H    6
#define PKT_I_AY_L    7
#define PKT_I_AZ_H    8
#define PKT_I_AZ_L    9
#define PKT_I_SPO2    10
#define PKT_SIZE      11

/* ============================================================
 * Enumerations
 * ============================================================ */

/**
  * @brief Node status from personal node
  */
typedef enum {
    NODE_STATUS_OK = 0,      /* Normal operation */
    NODE_STATUS_FALL = 1,    /* Fall detected */
    NODE_STATUS_SOS = 2      /* SOS button pressed */
} NodeStatus_t;

/**
  * @brief Proximity level based on RSSI
  */
typedef enum {
    PROXIMITY_VERY_FAR = 0,
    PROXIMITY_FAR = 1,
    PROXIMITY_MEDIUM = 2,
    PROXIMITY_CLOSE = 3,
    PROXIMITY_VERY_CLOSE = 4
} ProximityLevel_t;

/**
  * @brief UI state for LCD display
  */
typedef enum {
    UI_LIST_VIEW = 0,        /* List of all nodes */
    UI_DETAIL_VIEW = 1       /* Detail view of selected node */
} UI_State_t;

/* ============================================================
 * Data Structures
 * ============================================================ */

/**
  * @brief Personal node packet structure (11 bytes)
  * @note  Matches personal node packet format
  */
typedef struct {
    uint8_t pktType;      /* Packet type (PKT_TYPE_FULL) */
    uint8_t nodeId;       /* Node ID (1-4) */
    uint8_t status;       /* NodeStatus_t */
    uint8_t heartRate;    /* BPM (0 = invalid) */
    uint8_t spo2;         /* SpO2 percentage (0 = invalid) */
    int16_t accX;         /* X-axis acceleration (mg) */
    int16_t accY;         /* Y-axis acceleration (mg) */
    int16_t accZ;         /* Z-axis acceleration (mg) */
} __attribute__((packed)) PersonalNodePacket_t;

/**
  * @brief Tracked node information with metadata
  */
typedef struct {
    uint8_t nodeId;            /* Node ID (1-4) */
    NodeStatus_t status;       /* Current status */
    uint8_t urgentStatus;      /* Original status that triggered urgency */
    uint8_t heartRate;         /* Last heart rate (BPM) */
    uint8_t spo2;              /* Last SpO2 (%) */
    int16_t accX;              /* Last X acceleration (mg) */
    int16_t accY;              /* Last Y acceleration (mg) */
    int16_t accZ;              /* Last Z acceleration (mg) */
    int16_t rssi;              /* Smoothed RSSI (dBm) */
    int8_t snr;                /* SNR of last packet */
    ProximityLevel_t proximity; /* Proximity level */
    uint32_t lastSeenTime;     /* HAL_GetTick() timestamp */
    uint8_t isActive;          /* 1 if node has been seen recently */
    uint8_t packetCount;       /* Number of packets received */
    uint8_t isUrgent;          /* 1 if SOS or FALL status */
    uint32_t urgentStartTime;  /* When urgency started */
} TrackedNode_t;

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/* Initialization and main processing */
void Scanner_Init(void);
void Scanner_Process(void);

/* Node data access */
TrackedNode_t* Scanner_GetNode(uint8_t nodeId);
void Scanner_GetAllActiveNodes(TrackedNode_t* nodes, uint8_t* count);
uint8_t Scanner_GetClosestNode(void);
uint8_t Scanner_GetMostUrgentNode(void);

/* Utility functions */
ProximityLevel_t Scanner_RssiToProximity(int16_t rssi);
const char* Scanner_ProximityToString(ProximityLevel_t proximity);
const char* Scanner_StatusToString(NodeStatus_t status);

#endif /* __SCANNER_APP_H__ */