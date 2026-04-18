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

/* Operating modes */
typedef enum {
    MODE_LORAWAN = 0,
    MODE_RAW_LORA_RX = 1
} OperatingMode_t;

/* Personal node packet structure (match your document 3) */
typedef struct {
    uint8_t pktType;      // PKT_TYPE_FULL
    uint8_t nodeId;       // MY_NODE_ID
    uint8_t status;       // STATUS_OK, STATUS_FALL, STATUS_SOS
    uint8_t heartRate;    // BPM or 0/255
    int16_t accX;
    int16_t accY;
    int16_t accZ;
} PersonalNodePacket_t;

/* Function prototypes */
void DualMode_Init(void);
void DualMode_Switch(OperatingMode_t newMode);
OperatingMode_t DualMode_GetCurrent(void);
uint8_t DualMode_IsLoRaWANSuspended(void);
void DualMode_Process(void);
PersonalNodePacket_t* DualMode_GetLastPersonalNodeData(void);

#endif /* __DUAL_MODE_H__ */