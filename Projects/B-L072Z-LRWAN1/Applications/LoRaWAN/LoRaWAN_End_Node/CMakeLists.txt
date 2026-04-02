cmake_minimum_required(VERSION 3.20)
project(LoRaWAN_End_Node C ASM)

# ── Root of PFE2 ─────────────────────────────────────────────────────────────
set(PFE2_ROOT "C:/Users/malek/Desktop/PFE2")

set(DRIVERS      ${PFE2_ROOT}/Drivers)
set(MIDDLEWARES  ${PFE2_ROOT}/Middlewares)
set(UTILITIES    ${PFE2_ROOT}/Utilities)
set(CMAKE_OBJCOPY "C:/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-objcopy.exe")
set(CMAKE_SIZE    "C:/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-size.exe")

# ── MCU flags ─────────────────────────────────────────────────────────────────
set(MCU_FLAGS -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft)

add_compile_options(${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections -Os)

# ── Compiler definitions ──────────────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32L072xx
    USE_B_L072Z_LRWAN1
    CMWX1ZZABZ0XX
    LORAWAN_KMS=0
)

# ── Include paths ─────────────────────────────────────────────────────────────
include_directories(
    # Application
    Core/Inc
    LoRaWAN/App
    LoRaWAN/Target

    # HAL
    ${DRIVERS}/STM32L0xx_HAL_Driver/Inc
    ${DRIVERS}/STM32L0xx_HAL_Driver/Inc/Legacy

    # CMSIS
    ${DRIVERS}/CMSIS/Device/ST/STM32L0xx/Include
    ${DRIVERS}/CMSIS/Core/Include

    # BSP
    ${DRIVERS}/BSP/B-L072Z-LRWAN1
    ${DRIVERS}/BSP/CMWX1ZZABZ_0xx

    # LoRaWAN Middleware
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Conf
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Crypto
    ${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler
    ${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Mac
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/Region
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Utilities

    # SubGHz radio
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276

    # Utilities
    ${UTILITIES}/lpm/tiny_lpm
    ${UTILITIES}/sequencer
    ${UTILITIES}/timer
    ${UTILITIES}/misc
    ${UTILITIES}/trace/adv_trace
)

# ── Source files ──────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    # Application core
    "Core/Src/*.c"
    "LoRaWAN/App/*.c"
    "LoRaWAN/Target/*.c"
    "STM32CubeIDE/syscalls.c"
    "STM32CubeIDE/startup_stm32l072xx.s"

    # HAL
    "${DRIVERS}/STM32L0xx_HAL_Driver/Src/*.c"

    # BSP
    "${DRIVERS}/BSP/B-L072Z-LRWAN1/*.c"
    "${DRIVERS}/BSP/CMWX1ZZABZ_0xx/*.c"

    # LoRaWAN crypto
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Crypto/*.c"

    # LoRaWAN Mac
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/*.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/Region/*.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Utilities/*.c"

    # SubGHz radio
    "${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276/*.c"

    # Utilities
    "${UTILITIES}/lpm/tiny_lpm/*.c"
    "${UTILITIES}/sequencer/*.c"
    "${UTILITIES}/timer/*.c"
    "${UTILITIES}/misc/*.c"
    "${UTILITIES}/trace/adv_trace/*.c"
)

# ── LmHandler — explicit files only, no FUOTA/fragmentation/mcast ────────────
list(APPEND SOURCES
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/LmHandler.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/NvmDataMgmt.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages/LmhpClockSync.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages/LmhpCompliance.c"
    # Excluded — require FUOTA app-side implementations not present in End Node:
    # LmhpPackagesRegistration.c
    # LmhpFragmentation.c
    # LmhpRemoteMcastSetup.c
    # FragDecoder.c
)

# ── Exclude template files ────────────────────────────────────────────────────
list(FILTER SOURCES EXCLUDE REGEX ".*_template\\.c$")

# ── Linker script ─────────────────────────────────────────────────────────────
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32CubeIDE/cmwx1zzabz_0xx/STM32L072CZTx_FLASH.ld)

# ── ELF target ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    #-lc -lm -lnosys
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
)

# ── Post-build ────────────────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating .bin and printing size"
)