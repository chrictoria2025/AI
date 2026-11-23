# Raspberry Pi Pico W target configuration (placeholder)
#
# Requirements:
#   - pico-sdk installed and PICO_SDK_PATH set
#   - arm-none-eabi-gcc toolchain
#   - FreeRTOS-Kernel (SMP port for RP2040)
#
# Usage:
#   cmake -B build -DBUILD_TARGET=pico -DPICO_SDK_PATH=/path/to/pico-sdk
#
# Target: ARM Cortex-M0+ dual-core @ 133 MHz
# RAM: 264 KB SRAM
# Flash: 2 MB (W25Q16JV)

set(UAV_PLATFORM_DEFINES
    UAV_TARGET_PICO=1
    configUSE_PREEMPTION=1
    configTICK_RATE_HZ=1000
)
