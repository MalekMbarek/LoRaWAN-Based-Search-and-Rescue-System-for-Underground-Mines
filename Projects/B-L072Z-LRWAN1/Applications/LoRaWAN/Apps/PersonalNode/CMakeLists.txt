# ============================================================
# CMakeLists.txt — PersonalNode
# Personal node with fall detection, SOS, HR (MAX30102) and SpO2
# Transmits via Raw LoRa to ScannerNode
# ============================================================
cmake_minimum_required(VERSION 3.20)
project(PersonalNode)

# ── Paths ─────────────────────────────────────────────────────────────────────
get_filename_component(PFE2_ROOT
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../..
    ABSOLUTE
)

set(DRIVERS       ${PFE2_ROOT}/Drivers)
set(MIDDLEWARES   ${PFE2_ROOT}/Middlewares)
set(UTILITIES     ${PFE2_ROOT}/Utilities)

# ── Toolchain ─────────────────────────────────────────────────────────────────
# Toolchain prefix is passed via -DTOOLCHAIN_PREFIX or found in PATH
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy)
find_program(CMAKE_SIZE    arm-none-eabi-size)

# ── MCU Flags ─────────────────────────────────────────────────────────────────
set(MCU_FLAGS -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft)
add_compile_options(
    ${MCU_FLAGS}
    -Wall
    -fdata-sections
    -ffunction-sections
    -Os
)

# ── Compiler Definitions ──────────────────────────────────────────────────────
add_compile_definitions(
    USE_HAL_DRIVER
    STM32L072xx
    USE_B_L072Z_LRWAN1
    CMWX1ZZABZ0XX
    LORAWAN_KMS=0
    PERSONAL_NODE           # Enables #ifdef PERSONAL_NODE sections
    REGION_EU868            # EU868 region for personal node
)

# ── Include Paths ─────────────────────────────────────────────────────────────
include_directories(
    # Application-specific
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
    ${DRIVERS}/BSP/Components/lsm303agr   # LSM303AGR accelerometer

    # SubGHz Phy (SX1276 LoRa raw - not the full LoRaWAN stack)
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276

    # Utilities
    ${UTILITIES}/lpm/tiny_lpm
    ${UTILITIES}/sequencer
    ${UTILITIES}/timer
    ${UTILITIES}/misc
    ${UTILITIES}/trace/adv_trace
)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    # Application-specific
    "LoRaWAN/App/app_lorawan.c"
    "LoRaWAN/App/lora_app.c"
    "LoRaWAN/Target/*.c"
    "Core/Src/*.c"
    "STM32CubeIDE/syscalls.c"
    "STM32CubeIDE/startup_stm32l072xx.s"

    # HAL
    "${DRIVERS}/STM32L0xx_HAL_Driver/Src/*.c"

    # BSP Board + Radio Module
    "${DRIVERS}/BSP/B-L072Z-LRWAN1/*.c"
    "${DRIVERS}/BSP/CMWX1ZZABZ_0xx/*.c"

    # BSP Accelerometer (LSM303AGR)
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr.c"
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr_reg.c"

    # SubGHz Phy (SX1276 LoRa raw driver)
    "${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276/*.c"

    # Utilities
    "${UTILITIES}/lpm/tiny_lpm/*.c"
    "${UTILITIES}/sequencer/*.c"
    "${UTILITIES}/timer/*.c"
    "${UTILITIES}/misc/*.c"
    "${UTILITIES}/trace/adv_trace/*.c"
)

# Exclude template files
list(FILTER SOURCES EXCLUDE REGEX ".*_template\\.c$")

# ── Linker Script ─────────────────────────────────────────────────────────────
set(LINKER_SCRIPT 
    ${CMAKE_CURRENT_LIST_DIR}/STM32CubeIDE/cmwx1zzabz_0xx/STM32L072CZTx_FLASH.ld
)

# ── ELF Target ────────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -lc
    -lm
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
    COMMENT "Generating .bin and printing size"
)

# ── Print build info ──────────────────────────────────────────────────────────
message(STATUS "PersonalNode Configuration:")
message(STATUS "  PFE2 Root:    ${PFE2_ROOT}")
message(STATUS "  Drivers:      ${DRIVERS}")
message(STATUS "  Middlewares:  ${MIDDLEWARES}")
message(STATUS "  Utilities:    ${UTILITIES}")
message(STATUS "  Linker:       ${LINKER_SCRIPT}")