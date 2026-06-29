/**
  @page StaticNode Readme file

  @verbatim
  ******************** (C) COPYRIGHT 2023 StaticNode Project *******************
  * @file    Applications/LoRaWAN/StaticNode/readme.txt
  * @author  Your Name
  * @brief   Fixed monitoring station with environmental sensors, gas detection,
  *          motion monitoring and LoRaWAN connectivity
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

This directory contains the implementation of StaticNode, a fixed monitoring 
station designed for mine environments. The device continuously monitors 
environmental conditions and transmits data via LoRaWAN using Cayenne LPP format.

The StaticNode features:
  - **Environmental Monitoring**: HTS221 sensor for temperature and humidity
  - **Barometric Pressure**: LPS22HB sensor for atmospheric pressure
  - **Gas Detection**: MQ2 sensor for combustible gas (CH4) detection
  - **Motion Monitoring**: LSM303AGR accelerometer for vibration and tilt
  - **Audible Alerts**: Buzzer with configurable alert patterns
  - **LoRaWAN Connectivity**: OTAA activation with Cayenne LPP payload
  - **Dual Mode Operation**: LoRaWAN + Raw LoRa RX for personal node alerts

The device operates as follows:
  1. Reads environmental sensors (T, H, P) every 5 seconds
  2. Reads accelerometer for vibration/tilt detection
  3. Reads MQ2 gas sensor with 30s warmup period
  4. Processes alert conditions and triggers buzzer
  5. Transmits data via LoRaWAN every 10 seconds
  6. Switches to Raw LoRa RX mode every 30 seconds to listen for PersonalNode alerts
  7. Returns to LoRaWAN mode on emergency detection

@par Keywords

Applications, StaticNode, LoRaWAN, OTAA, Cayenne LPP, Gas Detection, MQ2, Environmental Monitoring, HTS221, LPS22HB, LSM303AGR, Buzzer, B-L072Z-LRWAN1

@par Directory contents

### Core/Inc (Header Files)
  - **app_config.h** - Central configuration (thresholds, pins, sensor parameters)
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
  - **sys_sensors.c** - Sensor drivers (HTS221, LPS22HB, LSM303AGR, MQ2, Buzzer)
  - **sys_debug.c** - Debug pins for RealTime debugging
  - **stm32l0xx_it.c** - Interrupt Service Routines
  - **stm32_lpm_if.c** - Low power mode functions
  - **stm32l0xx_hal_msp.c** - MSP Initialization and de-Initialization
  - **system_stm32l0xx.c** - CMSIS Cortex-M0+ System Source File

### LoRaWAN/App (Application Files)
  - **app_lorawan.c** - LoRaWAN application wrapper
  - **app_lorawan.h** - Header for app_lorawan.c
  - **lora_app.c** - Main application logic (timers, LoRaWAN, Cayenne LPP)
  - **lora_app.h** - Application configuration (LoRaWAN parameters, ports)
  - **CayenneLpp.c** - Cayenne LPP protocol implementation
  - **CayenneLpp.h** - Header for CayenneLpp.c
  - **dual_mode.c** - Dual mode (LoRaWAN/Raw LoRa) implementation
  - **dual_mode.h** - Header for dual_mode.c

### LoRaWAN/Target (BSP and Radio Drivers)
  - **b_l072z_lrwan1_bus.c** - BSP BUS IO driver
  - **b_l072z_lrwan1_bus.h** - Header for BSP BUS IO driver
  - **radio_board_if.h** - Radio interface configuration
  - **radio_conf.h** - Radio configuration
  - **cmwx1zzabz_0xx_conf.h** - Shield configuration (pin mapping)
  - **iks01a2_conf.h** - MEMS components bus interfaces
  - **lorawan_conf.h** - LoRaWAN middleware instances

### STM32CubeIDE
  - **startup_stm32l072xx.s** - Startup assembly file
  - **syscalls.c** - System call implementations

@par Hardware and Software Environment

### Hardware Requirements
  - **MCU**: STM32L072CZ (Cortex-M0+)
  - **Board**: B-L072Z-LRWAN1 RevC
  - **Temperature/Humidity**: HTS221 (I2C address 0x5F)
  - **Barometric Pressure**: LPS22HB (I2C address 0xBB)
  - **Accelerometer**: LSM303AGR (I2C address 0x32)
  - **Gas Sensor**: MQ2 (ADC on PA0)
  - **Buzzer**: PB5 (GPIO)
  - **Radio**: SX1276 LoRa module

### Pin Mapping
  | Pin | Function | Description |
  |-----|----------|-------------|
  | PA0 | MQ2_AOUT | Gas sensor analog input (ADC channel 0) |
  | PB5 | BUZZER | Audible alert output (GPIO) |
  | PB8 | I2C1_SCL | I2C clock for sensors |
  | PB9 | I2C1_SDA | I2C data for sensors |

### Software Requirements
  - ARM GCC Toolchain (arm-none-eabi)
  - CMake 3.20+ with Ninja build system
  - STM32CubeMX 6.0+ (for configuration)

### Setup Instructions
  1. Connect the B-L072Z-LRWAN1 board to your PC via USB
  2. Ensure ST-LINK jumpers (CN8) are fitted
  3. Connect MQ2 sensor: AOUT→PA0, VCC→3.3V, GND→GND
  4. Connect I2C sensors to PB8 (SCL) and PB9 (SDA)
  5. Verify buzzer connection to PB5
  6. Open terminal (115200 baud, 8N1)

@par Configuration Files

### app_config.h - Key Parameters
  ```c
  // I2C Sensor Addresses
  #define HTS221_I2C_ADDRESS          0x5FU     // Temperature/Humidity
  #define LPS22HB_I2C_ADDRESS         0xBBU     // Barometric Pressure
  #define ACC_ADDR                    0x32U     // Accelerometer

  // Motion Thresholds
  #define THRESHOLD_VIBRATION_MG          200.0f  // Warning level
  #define THRESHOLD_VIBRATION_CRITICAL_MG 500.0f  // Critical level
  #define THRESHOLD_TILT_DEG               45.0f  // Warning level
  #define THRESHOLD_TILT_CRITICAL_DEG      70.0f  // Critical level

  // MQ2 Gas Sensor
  #define MQ2_RL_KOHM                 20.0f         // Load resistor (kOhm)
  #define MQ2_RO_CLEAN_AIR_RATIO      9.83f         // Rs/Ro in clean air
  #define MQ2_PREHEAT_MS              30000U        // Warmup time (30s)
  #define MQ2_THRESHOLD_WARN_PPM      300.0f        // Warning level
  #define MQ2_THRESHOLD_CRIT_PPM      1000.0f       // Critical level
  #define MQ2_THRESHOLD_EXPLOSIVE_PPM 5000.0f       // Explosive level

  // Buzzer Levels
  #define BUZZER_LEVEL_OK             0             // 2 short beeps
  #define BUZZER_LEVEL_WARN           1             // 3 short beeps
  #define BUZZER_LEVEL_CRIT           2             // 5 long beeps
  #define BUZZER_LEVEL_SOS            3             // SOS pattern