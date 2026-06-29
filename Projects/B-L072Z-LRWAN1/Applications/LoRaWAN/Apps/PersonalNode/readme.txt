/**
  @page PersonalNode Readme file

  @verbatim
  ******************** (C) COPYRIGHT 2023 PersonalNode Project *******************
  * @file    Applications/LoRaWAN/PersonalNode/readme.txt
  * @author  Your Name
  * @brief   Personal safety wearable with fall detection, SOS, HR monitoring
  *          and raw LoRa transmission to ScannerNode
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Your Name.
  * All rights reserved.</center></h2>
  *
  * This project is part of the PFE2 - Mine Safety System.
  *
  ******************************************************************************
  @endverbatim

@par Description

This directory contains the implementation of PersonalNode, a wearable personal 
safety device designed for mine workers. The device monitors vital signs and 
movement, detects falls and emergencies, and transmits data via raw LoRa to a 
central scanner node.

The PersonalNode features:
  - **Fall Detection**: LSM303AGR accelerometer with free-fall + impact algorithm
  - **Emergency SOS**: Physical button trigger for immediate alert
  - **Vital Signs Monitoring**: MAX30102 sensor for heart rate (BPM) and SpO2
  - **Real-time Transmission**: Raw LoRa transmission every 10 seconds
  - **Low Power Operation**: Optimized for battery-powered wearable use
  - **Visual Indicators**: SOS LED and Heartbeat LED for status feedback

The device operates as follows:
  1. Continuously reads accelerometer data at 100Hz
  2. Reads HR/SpO2 data from MAX30102 at 10ms intervals
  3. Processes fall detection algorithm (free-fall + impact)
  4. Monitors SOS button state via external interrupt
  5. Transmits packet via raw LoRa every 10 seconds
  6. LED indicators show device status

@par Keywords

Applications, PersonalNode, LoRa, Raw LoRa, Fall Detection, SOS, Heart Rate, SpO2, MAX30102, LSM303AGR, B-L072Z-LRWAN1

@par Directory contents

### Core/Inc (Header Files)
  - **app_config.h** - Central configuration (thresholds, pins, intervals)
  - **main.h** - Header for main.c file
  - **platform.h** - General HW instances configuration
  - **sys_app.h** - Function prototypes for sys_app.c
  - **sys_conf.h** - Applicative configuration (debug, trace, low power)
  - **sys_debug.h** - Configuration of the debug.c instances
  - **sys_sensors.h** - Header for sensor application
  - **stm32l0xx_hal_conf.h** - HAL configuration file
  - **stm32l0xx_it.h** - Interrupt handlers headers
  - **stm32_lpm_if.h** - Header for Low Power Manager interface
  - **utilities_def.h** - Definitions for modules requiring utilities

### Core/Src (Source Files)
  - **main.c** - Main application entry point
  - **sys_app.c** - System initialization (GPIO, LPM, timers, RTC)
  - **sys_sensors.c** - Sensor drivers (LSM303AGR, MAX30102 with HR/SpO2)
  - **sys_debug.c** - Debug pins for RealTime debugging
  - **stm32l0xx_it.c** - Interrupt Service Routines
  - **stm32_lpm_if.c** - Low power mode functions
  - **stm32l0xx_hal_msp.c** - MSP Initialization and de-Initialization
  - **system_stm32l0xx.c** - CMSIS Cortex-M0+ System Source File

### LoRaWAN/App (Application Files)
  - **app_lorawan.c** - LoRaWAN application wrapper
  - **app_lorawan.h** - Header for app_lorawan.c
  - **lora_app.c** - Main application logic (timers, tasks, SOS, fall detection)
  - **lora_app.h** - Application configuration (LoRa parameters, ports)
  - **CayenneLpp.c** - Cayenne LPP protocol implementation
  - **CayenneLpp.h** - Header for CayenneLpp.c

### LoRaWAN/Target (BSP and Radio Drivers)
  - **b_l072z_lrwan1_bus.c** - BSP BUS IO driver
  - **b_l072z_lrwan1_bus.h** - Header for BSP BUS IO driver
  - **radio_board_if.h** - Radio interface configuration
  - **radio_conf.h** - Radio configuration
  - **cmwx1zzabz_0xx_conf.h** - Shield configuration (pin mapping)

### STM32CubeIDE
  - **startup_stm32l072xx.s** - Startup assembly file
  - **syscalls.c** - System call implementations

@par Hardware and Software Environment

### Hardware Requirements
  - **MCU**: STM32L072CZ (Cortex-M0+)
  - **Board**: B-L072Z-LRWAN1 RevC
  - **Accelerometer**: LSM303AGR (I2C address 0x32)
  - **Heart Rate/SpO2**: MAX30102 (I2C address 0x57)
  - **Radio**: SX1276 LoRa module
  - **SOS Button**: PA0 (external interrupt, falling edge)
  - **LEDs**: PA5 (SOS LED), PB13 (Heartbeat LED)

### Pin Mapping
  | Pin | Function | Description |
  |-----|----------|-------------|
  | PA0 | SOS_BUTTON | External interrupt (falling edge) with pull-up |
  | PA5 | LED_SOS | SOS alert indicator |
  | PB13 | LED_HEARTBEAT | Heartbeat indicator |
  | PB8 | I2C1_SCL | I2C clock for sensors |
  | PB9 | I2C1_SDA | I2C data for sensors |

### Software Requirements
  - ARM GCC Toolchain (arm-none-eabi)
  - CMake 3.20+ with Ninja build system
  - STM32CubeMX 6.0+ (for configuration)

### Setup Instructions
  1. Connect the B-L072Z-LRWAN1 board to your PC via USB
  2. Ensure ST-LINK jumpers (CN8) are fitted
  3. Connect MAX30102 module to I2C bus (PB8/PB9)
  4. Connect LSM303AGR module to I2C bus (PB8/PB9)
  5. Verify SOS button connection to PA0
  6. Open terminal (115200 baud, 8N1)

@par Configuration Files

### app_config.h - Key Parameters
  ```c
  // Accelerometer
  #define ACC_SENSITIVITY_MG          0.98f     // mg/LSB @ ±8g
  #define ACC_CTRL1_VAL               0x57U     // ODR=100Hz, XYZ enabled
  #define ACC_CTRL4_VAL               0x88U     // BDU=1, FS=±8g, HR=1

  // Fall Detection
  #define PN_FREE_FALL_THRESHOLD_MG   500.0f    // Free-fall threshold
  #define PN_IMPACT_THRESHOLD_MG      2500.0f   // Impact threshold
  #define PN_FALL_WINDOW_MS           500U      // Detection window (ms)

  // HR/SpO2
  #define PN_MAX_SAMPLES              100       // Sample buffer size
  #define PN_BPM_BUFFER_SIZE          5         // BPM averaging window
  #define PN_SPO2_MIN_SAMPLES         30        // Minimum samples for SpO2

  // LoRa
  #define PN_RF_FREQUENCY             868000000 // EU868 band (Hz)
  #define PN_SPREADING_FACTOR         7         // SF7 for fast transmission
  #define PN_TX_PERIOD_MS             10000     // Transmit interval (10s)