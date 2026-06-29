/**
  @page ScannerNode Readme file

  @verbatim
  ******************** (C) COPYRIGHT 2023 ScannerNode Project *******************
  * @file    Applications/LoRaWAN/ScannerNode/readme.txt
  * @author  Your Name
  * @brief   Portable scanner for personal node tracking with LCD display,
  *          joystick navigation and audible proximity alerts
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

This directory contains the implementation of ScannerNode, a portable handheld 
device that receives and displays data from PersonalNode wearables. It provides 
real-time visualization of worker status, location proximity, and emergency 
alerts via LCD display and audible buzzer.

The ScannerNode features:
  - **Raw LoRa Reception**: Receives PersonalNode transmissions at 868MHz
  - **Node Tracking**: Tracks up to 4 personal nodes simultaneously
  - **Proximity Estimation**: Calculates distance from RSSI using log-distance model
  - **LCD Display**: 20x4 I2C display with list and detail views
  - **Joystick Navigation**: Analog X/Y with button for scrolling and selection
  - **Audible Alerts**: Buzzer with proximity-based patterns (100ms to 2s intervals)
  - **Emergency Detection**: Immediate visual/audio alerts for SOS/FALL status
  - **Urgent Hold**: Maintains emergency status display for 40 seconds

The device operates as follows:
  1. Continuously listens for PersonalNode transmissions on Raw LoRa
  2. Parses incoming packets (11 bytes, matching PersonalNode format)
  3. Updates tracked nodes with RSSI, SNR, and sensor data
  4. Displays node list on LCD with proximity indicators
  5. Allows navigation using joystick (UP/DOWN to scroll, LEFT/RIGHT for detail pages)
  6. Generates buzzer patterns based on node proximity and urgency
  7. Highlights emergency nodes (SOS > FALL > OK)

@par Keywords

Applications, ScannerNode, Raw LoRa, Personal Node Tracking, RSSI, Proximity Detection, LCD Display, Joystick, Buzzer, B-L072Z-LRWAN1

@par Directory contents

### Core/Inc (Header Files)
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
  - **main.c** - Main application entry point (I2C, GPIO, ADC init)
  - **sys_app.c** - System initialization (GPIO, LPM, timers, RTC)
  - **sys_debug.c** - Debug pins for RealTime debugging
  - **stm32l0xx_it.c** - Interrupt Service Routines
  - **stm32_lpm_if.c** - Low power mode functions
  - **stm32l0xx_hal_msp.c** - MSP Initialization and de-Initialization
  - **system_stm32l0xx.c** - CMSIS Cortex-M0+ System Source File

### LoRaWAN/App (Application Files)
  - **scanner_app.c** - Main application logic (node tracking, UI, alerts)
  - **scanner_app.h** - Application configuration and data structures
  - **lcd_display.c** - LCD 20x4 I2C driver (4-bit mode, PCF8574)
  - **lcd_display.h** - Header for lcd_display.c
  - **buzzer.c** - Buzzer control with patterns
  - **buzzer.h** - Header for buzzer.c
  - **joystick.c** - Analog (ADC) and digital joystick driver
  - **joystick.h** - Header for joystick.c

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
  - **Display**: LCD 20x4 with I2C backpack (PCF8574)
  - **Radio**: SX1276 LoRa module
  - **Joystick**: Analog X/Y (PA0, PA2) + Digital button (PA3)
  - **Buzzer**: PA8 (GPIO)

### Pin Mapping
  | Pin | Function | Description |
  |-----|----------|-------------|
  | PA0 | JOY_X | Joystick X-axis (ADC channel 0) |
  | PA2 | JOY_Y | Joystick Y-axis (ADC channel 2) |
  | PA3 | JOY_SW | Joystick button (input with pull-up) |
  | PA8 | BUZZER | Audible alert output (GPIO) |
  | PB8 | I2C1_SCL | I2C clock for LCD |
  | PB9 | I2C1_SDA | I2C data for LCD |

### Software Requirements
  - ARM GCC Toolchain (arm-none-eabi)
  - CMake 3.20+ with Ninja build system
  - STM32CubeMX 6.0+ (for configuration)

### Setup Instructions
  1. Connect the B-L072Z-LRWAN1 board to your PC via USB
  2. Ensure ST-LINK jumpers (CN8) are fitted
  3. Connect LCD I2C module to PB8 (SCL) and PB9 (SDA)
  4. Connect joystick: X→PA0, Y→PA2, SW→PA3
  5. Connect buzzer to PA8
  6. Open terminal (115200 baud, 8N1)
  7. Ensure PersonalNode is transmitting on 868MHz, SF7

@par Configuration Files

### scanner_app.h - Key Parameters
  ```c
  // Node Tracking
  #define MAX_TRACKED_NODES       4          // Maximum nodes to track

  // RSSI Thresholds for Proximity
  #define RSSI_VERY_CLOSE         -40        // < 2 meters
  #define RSSI_CLOSE              -60        // 2-5 meters
  #define RSSI_MEDIUM             -80        // 5-15 meters
  #define RSSI_FAR                -100       // 15-50 meters

  // Buzzer Patterns (ms)
  #define BUZZ_VERY_CLOSE_PERIOD  100        // Fast beeping
  #define BUZZ_CLOSE_PERIOD       300        // Medium beeping
  #define BUZZ_MEDIUM_PERIOD      800        // Slow beeping
  #define BUZZ_FAR_PERIOD         2000       // Very slow beeping

  // Timing
  #define NODE_TIMEOUT_MS         60000      // Node inactive after 60s
  #define URGENT_HOLD_TIME_MS     40000      // Hold urgent status for 40s
  #define LCD_UPDATE_INTERVAL_MS  500        // LCD refresh rate
  #define BUZZER_UPDATE_INTERVAL_MS 100      // Buzzer update rate

  // Joystick Thresholds
  #define JOY_HIGH                3000       // High threshold
  #define JOY_LOW                 1000       // Low threshold
  #define JOY_CENTER_LOW          1600       // Center deadzone low
  #define JOY_CENTER_HIGH         2400       // Center deadzone high