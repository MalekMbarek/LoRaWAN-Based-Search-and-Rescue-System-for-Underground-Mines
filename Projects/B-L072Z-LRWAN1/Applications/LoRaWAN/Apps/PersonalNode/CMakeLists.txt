# ============================================================
# CMakeLists.txt — PersonalNode
# Architecture identique au nœud statique (doc 14)
# LoRa raw TX : Fall detection + SOS + HR (MAX30102) + SpO2
# ============================================================
cmake_minimum_required(VERSION 3.20)
project(PersonalNode)

# ── Paths ─────────────────────────────────────────────────────────────────────
# PFE2_ROOT pointe vers la racine du dépôt PFEv3
# (6 niveaux au-dessus de Apps/PersonalNode/ selon la structure du nœud statique)
get_filename_component(PFE2_ROOT
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../
    ABSOLUTE
)


set(COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

set(DRIVERS     ${PFE2_ROOT}/Drivers)
set(MIDDLEWARES ${PFE2_ROOT}/Middlewares)
set(UTILITIES   ${PFE2_ROOT}/Utilities)


# ── Tools ─────────────────────────────────────────────────────────────────────

#balkis
# set(TOOLCHAIN_PREFIX "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10")
# set(CMAKE_OBJCOPY "${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-objcopy.exe")
# set(CMAKE_SIZE    "${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-size.exe")
#malek
set(TOOLCHAIN_PREFIX "C:/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi")
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy)
find_program(CMAKE_SIZE    arm-none-eabi-size)

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
    PERSONAL_NODE           # permet #ifdef PERSONAL_NODE si besoin
    REGION_EU868            # EU868 uniquement pour le nœud personnel
)

# ── Include paths ─────────────────────────────────────────────────────────────
include_directories(
    # App-specific
    LoRaWAN/App
    LoRaWAN/Target
    Core/Inc

    # HAL
    ${DRIVERS}/STM32L0xx_HAL_Driver/Inc
    ${DRIVERS}/STM32L0xx_HAL_Driver/Inc/Legacy

    # CMSIS
    ${DRIVERS}/CMSIS/Device/ST/STM32L0xx/Include
    ${DRIVERS}/CMSIS/Core/Include

    # BSP
    ${DRIVERS}/BSP/B-L072Z-LRWAN1
    ${DRIVERS}/BSP/CMWX1ZZABZ_0xx
    ${DRIVERS}/BSP/Components/Common
    ${DRIVERS}/BSP/Components/lsm303agr   # AccéléromètreLSM303AGR

    # SubGHz Phy (SX1276 LoRa raw -- pas la stack LoRaWAN complète)
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276

    # Utilities ST
    ${UTILITIES}/lpm/tiny_lpm
    ${UTILITIES}/sequencer
    ${UTILITIES}/timer
    ${UTILITIES}/misc
    ${UTILITIES}/trace/adv_trace
)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    # Application (nos fichiers)
    "LoRaWAN/App/app_lorawan.c"
    "LoRaWAN/App/lora_app.c"
    "LoRaWAN/Target/*.c"
    "Core/Src/*.c"
    "STM32CubeIDE/syscalls.c"
    "STM32CubeIDE/startup_stm32l072xx.s"

    # HAL
    "${DRIVERS}/STM32L0xx_HAL_Driver/Src/*.c"

    # BSP board + radio module
    "${DRIVERS}/BSP/B-L072Z-LRWAN1/*.c"
    "${DRIVERS}/BSP/CMWX1ZZABZ_0xx/*.c"

    # BSP capteur ACC (pas hts221/lps22hb/mq2 -- nœud personnel)
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr.c"
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr_reg.c"

    # SubGHz Phy (SX1276 driver LoRa raw)
    "${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276/*.c"

    # Utilities ST
    "${UTILITIES}/lpm/tiny_lpm/*.c"
    "${UTILITIES}/sequencer/*.c"
    "${UTILITIES}/timer/*.c"
    "${UTILITIES}/misc/*.c"
    "${UTILITIES}/trace/adv_trace/*.c"
)

# Exclure les fichiers template générés
list(FILTER SOURCES EXCLUDE REGEX ".*_template\\.c$")

# ── Linker script ─────────────────────────────────────────────────────────────
set(LINKER_SCRIPT
    ${CMAKE_CURRENT_LIST_DIR}/STM32CubeIDE/cmwx1zzabz_0xx/STM32L072CZTx_FLASH.ld)

# ── ELF target ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -lc -lm
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map,--cref
)

set_property(TARGET ${PROJECT_NAME}.elf PROPERTY C_STANDARD 11)

# ── Post-build ────────────────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.bin and printing size"
)