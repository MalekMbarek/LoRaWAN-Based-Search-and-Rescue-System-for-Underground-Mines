# LoRa/LoRaWAN-Based Search and Rescue System for Underground Mines

End-of-study project (PFE) for the National Engineering Degree in Infotronic Systems Engineering, **ENICarthage**, carried out at **SofiaTech** (Onetech Group). Academic year 2025-2026.

A complete IoT system that monitors miners and their environment in real time, detects emergencies (fall, SOS, hazardous gas), and guides rescue teams to victims **without GPS**, using LoRa RSSI-based localization.

> Underground, GPS is unavailable and GSM/Wi-Fi coverage is poor. LoRa/LoRaWAN offers long range at very low power, which makes it a good fit for this setting.

---

## System overview

```
Personal Nodes ──raw LoRa──► Static Node ──LoRaWAN──► Gateway ──► ChirpStack ──MQTT──► ThingsBoard
       │                         (sensors + buzzer)    (Dragino)   (decode LPP)         (dashboards)
       └──raw LoRa (RSSI)──► Portable Scanner (LCD + buzzer guidance)
```

| Subsystem | Role | Key hardware |
|---|---|---|
| **Personal Node** | Wearable. Heart rate, SpO2, fall detection, SOS button. Hourly beacon, immediate emergency packet. | B-L072Z-LRWAN1 (STM32L072CZ + SX1276), MAX30102, LSM303AGR |
| **Static Node** | Mains-powered. Environmental monitoring, receives personal node packets, uplinks via LoRaWAN. | I-NUCLEO-LRWAN1 shield (HTS221, LPS22HB, LSM303AGR), MQ-2, buzzer |
| **Portable Scanner** | Handheld rescue device. Continuous raw LoRa RX, RSSI to distance, urgency sorting, LCD and audio guidance. | STM32L072CZ + SX1276, I2C LCD (0x27), analog joystick, buzzer |
| **Gateway** | LoRaWAN packet relay. | Dragino LoRaWAN gateway (868 MHz) |
| **Cloud** | Network server, payload decoding, dashboards, alarms. | ChirpStack, ThingsBoard |

## Operating modes

- **Normal**: periodic monitoring and telemetry.
- **Alert**: abnormal environment (gas, temperature, vibration/tilt). Local buzzer and dashboard warning.
- **Emergency**: fall or SOS. Immediate high-priority packet and rescue notification.
- **Disaster**: large-scale incident. Autonomous scanner-based localization if the gateway is unavailable.

## Repository structure

```
.
├── cmake/                     # CMake sources (apps, boards, mac, peripherals, radio, system)
├── Drivers/                   # STM32 HAL (L0/L1/L4), CMSIS, BSP and sensor components
├── Middlewares/
│   ├── ST/STM32_Secure_Engine
│   └── Third_Party/           # LoRaWAN stack, mbedTLS, SubGHz_Phy (SX1276/SX1272/SX126x)
├── Utilities/                 # sequencer, timer, low-power manager, trace, misc
└── Projects/B-L072Z-LRWAN1/Applications/
    ├── LoRaWAN/Apps/
    │   ├── PersonalNode/      # Wearable node firmware (this project)
    │   ├── StaticNode/        # Environmental node and LoRaWAN uplink (this project)
    │   ├── ScannerNode/       # Rescue scanner firmware (this project)
    │   ├── LoRaWAN_End_Node/  # ST reference example
    │   └── LoRaWAN_AT_Slave/  # ST reference example
    └── SubGHz_Phy/SubGHz_Phy_PingPong/   # ST reference example
```

The three project-specific applications are `PersonalNode`, `StaticNode` and `ScannerNode`. Everything else is the unmodified STM32CubeL0 package and ST examples.

## Build and flash

The firmware is built with **CMake + Ninja** and the **Arm GNU toolchain** (`arm-none-eabi-gcc`) instead of the STM32CubeIDE generator. One source tree produces several firmware images, selected with the `APPLICATION` variable.

**Prerequisites:** CMake, Ninja, `arm-none-eabi-gcc` (Cortex-M0+), and an ST-Link flasher (STM32CubeProgrammer or OpenOCD).

```bash
# from Projects/B-L072Z-LRWAN1/Applications/LoRaWAN
cmake -S . -B build -G Ninja -DAPPLICATION=PersonalNode   # or StaticNode / ScannerNode
ninja -C build
```

The build produces `<App>.elf` and `<App>.bin`, and prints memory usage. Flash the `.bin` to the STM32L072 board, for example with STM32CubeProgrammer, or by copying it to the board's ST-Link mass-storage drive.

> Adjust the paths above if your root `CMakeLists.txt` lives elsewhere.

## Radio and protocol summary

**Raw LoRa** (personal node to static node / scanner): 868 MHz, BW 125 kHz, SF7, CR 4/5, 8-symbol preamble, SX1276 hardware CRC. Packet length is 11 bytes:

| Byte | Field | Description |
|---|---|---|
| 0 | `pktType` | `0x01` supervision beacon |
| 1 | `nodeId` | 1..4 |
| 2 | `status` | 0 = OK, 1 = FALL, 2 = SOS |
| 3 | `heartRate` | BPM (255 = sensor absent, 0 = invalid) |
| 4-9 | `accX/Y/Z` | int16 big-endian, mg |
| 10 | `spo2` | % (0 = invalid) |

**LoRaWAN** (static node to cloud): Class A, OTAA, AES-128, encoded as **Cayenne LPP**. Channels 0-4 carry pressure, temperature, humidity, motion alert and gas alert. Channels 5-16 carry the ID, status, heart rate and SpO2 of personal nodes 1-3.

**Localization:** log-distance path loss model, `d = d0 * 10^((RSSI(d0) - RSSI(d)) / (10 n))`, calibrated with RSSI(1 m) = -45 dBm and n = 2.7, plus exponential moving average smoothing and stale-node timeout.

## Cloud setup

1. Register the gateway and devices (OTAA: DevEUI, AppEUI, AppKey) in **ChirpStack**.
2. Add the custom JavaScript **Cayenne LPP payload decoder** to the device profile.
3. Forward decoded uplinks to **ThingsBoard** over MQTT (`v1/devices/me/telemetry`).
4. Import or build the dashboards: Overview, Alarms, Environmental Monitoring, Personal Node Monitoring and Localization (radar view).

## Results and limitations

The prototype validated reliable data transmission, immediate emergency alerts, and proximity estimation by the scanner.

- RSSI-based localization is only approximate underground (multipath, tunnel geometry, body shadowing).
- Only one static node was built, so full trilateration was not tested.
- The environmental sensor set is limited (no CO or O2 sensing).
- The estimated personal node autonomy is about 11 days on 3x AAA, dominated by continuous accelerometer and MAX30102 sampling.

## Future work

Multiple static nodes with trilateration, additional gas and O2 sensors, edge computing on static nodes, AI/ML predictive analytics in the cloud, and haptic/voice guidance on the scanner.

## Documentation

The full report (*LoRa/LoRaWAN-Based Search and Rescue System*) covers requirements, technology selection, hardware and software design, UML models, and dashboards. It is available in the `docs/` folder, if added.

## Author

**Malek Mbarek**, ENICarthage, Infotronic Systems Engineering.

- Academic supervisor: Montassar Najari (ENICarthage)
- Professional supervisor: Sabri Boussami (SofiaTech)

## License

Project-specific code: add your chosen license here (for example MIT). Third-party code in `Drivers/`, `Middlewares/` and `Utilities/` (STMicroelectronics HAL/CMSIS/BSP, mbedTLS, LoRaWAN stack and others) keeps its original licenses. See the license files in those folders.
