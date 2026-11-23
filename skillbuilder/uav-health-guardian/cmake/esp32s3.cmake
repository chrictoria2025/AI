# ESP32-S3 target configuration via ESP-IDF (placeholder)
#
# Requirements:
#   - ESP-IDF v5.x installed and sourced (. $IDF_PATH/export.sh)
#   - Xtensa toolchain (installed with ESP-IDF)
#   - FreeRTOS included with ESP-IDF
#
# Usage:
#   idf.py set-target esp32s3
#   idf.py build
#
# Target: Xtensa LX7 dual-core @ 240 MHz
# RAM: 512 KB SRAM + 8 MB PSRAM (optional)
# Flash: 8-16 MB (quad SPI)

set(UAV_PLATFORM_DEFINES
    UAV_TARGET_ESP32S3=1
    configUSE_PREEMPTION=1
    configTICK_RATE_HZ=1000
)
