# ============================================================
# CMakeLists.txt — StaticNode
# LoRaWAN end node with Cayenne LPP payload
# Environmental sensors: HTS221, LPS22HB, LSM303AGR, MQ2, Buzzer
# ============================================================
cmake_minimum_required(VERSION 3.20)
project(StaticNode)

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
    ${DRIVERS}/BSP/IKS01A2
    ${DRIVERS}/BSP/Components/Common
    ${DRIVERS}/BSP/Components/hts221
    ${DRIVERS}/BSP/Components/lps22hb
    ${DRIVERS}/BSP/Components/lsm6dsl
    ${DRIVERS}/BSP/Components/lsm303agr

    # LoRaWAN Middleware
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Conf
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Crypto
    ${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler
    ${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Mac
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/Region
    ${MIDDLEWARES}/Third_Party/LoRaWAN/Utilities

    # Radio
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
    "LoRaWAN/App/*.c"
    "Core/Src/*.c"
    "LoRaWAN/Target/*.c"
    "STM32CubeIDE/syscalls.c"
    "STM32CubeIDE/startup_stm32l072xx.s"

    # HAL
    "${DRIVERS}/STM32L0xx_HAL_Driver/Src/*.c"

    # BSP
    "${DRIVERS}/BSP/B-L072Z-LRWAN1/*.c"
    "${DRIVERS}/BSP/CMWX1ZZABZ_0xx/*.c"
    "${DRIVERS}/BSP/IKS01A2/*.c"

    # Sensor Components
    "${DRIVERS}/BSP/Components/hts221/hts221.c"
    "${DRIVERS}/BSP/Components/hts221/hts221_reg.c"
    "${DRIVERS}/BSP/Components/lps22hb/lps22hb.c"
    "${DRIVERS}/BSP/Components/lps22hb/lps22hb_reg.c"
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr.c"
    "${DRIVERS}/BSP/Components/lsm303agr/lsm303agr_reg.c"
    "${DRIVERS}/BSP/Components/lsm6dsl/lsm6dsl.c"
    "${DRIVERS}/BSP/Components/lsm6dsl/lsm6dsl_reg.c"

    # LoRaWAN Middleware
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Crypto/*.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/*.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Mac/Region/*.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/Utilities/*.c"
    "${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276/*.c"

    # Utilities
    "${UTILITIES}/lpm/tiny_lpm/*.c"
    "${UTILITIES}/sequencer/*.c"
    "${UTILITIES}/timer/*.c"
    "${UTILITIES}/misc/*.c"
    "${UTILITIES}/trace/adv_trace/*.c"
)

# LmHandler (exclude FUOTA files not needed for End Node)
list(APPEND SOURCES
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/LmHandler.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/NvmDataMgmt.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages/LmhpClockSync.c"
    "${MIDDLEWARES}/Third_Party/LoRaWAN/LmHandler/Packages/LmhpCompliance.c"
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

# ── Post-build ────────────────────────────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating .bin and printing size"
)

# ── Print build info ──────────────────────────────────────────────────────────
message(STATUS "StaticNode Configuration:")
message(STATUS "  PFE2 Root:    ${PFE2_ROOT}")
message(STATUS "  Drivers:      ${DRIVERS}")
message(STATUS "  Middlewares:  ${MIDDLEWARES}")
message(STATUS "  Utilities:    ${UTILITIES}")
message(STATUS "  Linker:       ${LINKER_SCRIPT}")