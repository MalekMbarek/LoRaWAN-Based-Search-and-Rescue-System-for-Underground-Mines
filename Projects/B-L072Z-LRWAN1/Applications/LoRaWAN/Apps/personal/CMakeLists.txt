cmake_minimum_required(VERSION 3.20)
project(personal)

get_filename_component(PFE2_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../../../
                       ABSOLUTE)

set(DRIVERS     ${PFE2_ROOT}/Drivers)
set(MIDDLEWARES ${PFE2_ROOT}/Middlewares)

set(TOOLCHAIN_PREFIX "C:/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-objcopy.exe")
set(CMAKE_SIZE    "${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-size.exe")

set(MCU_FLAGS -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft)
add_compile_options(${MCU_FLAGS} -Wall -fdata-sections -ffunction-sections -Os)

add_compile_definitions(USE_HAL_DRIVER STM32L072xx)

include_directories(
    Core/Inc
    ${DRIVERS}/STM32L0xx_HAL_Driver/Inc
    ${DRIVERS}/CMSIS/Device/ST/STM32L0xx/Include
    ${DRIVERS}/CMSIS/Core/Include
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276
)

set(SOURCES
    Core/Src/main.c
    Core/Src/stm32l0xx_it.c
    Core/Src/system_stm32l0xx.c
    STM32CubeIDE/syscalls.c
    STM32CubeIDE/startup_stm32l072xx.s

    # HAL drivers (minimum needed)
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_cortex.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_gpio.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_i2c.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_uart.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_rcc.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_rcc_ex.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_dma.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_flash.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_flash_ex.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pwr.c
    ${DRIVERS}/STM32L0xx_HAL_Driver/Src/stm32l0xx_hal_pwr_ex.c

    # Radio low-level driver
    ${MIDDLEWARES}/Third_Party/SubGHz_Phy/sx1276/sx1276.c
)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32CubeIDE/cmwx1zzabz_0xx/STM32L072CZTx_FLASH.ld)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -lc -lm
    -Wl,--gc-sections
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
)