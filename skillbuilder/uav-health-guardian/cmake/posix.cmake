# POSIX target configuration
# No special toolchain needed — uses host compiler
# Supports: macOS, Linux, Windows (MSVC/MinGW)

set(UAV_HAL_SOURCES
    src/hal/posix/PosixSensorHAL.cpp
)

set(UAV_PLATFORM_DEFINES
    UAV_TARGET_POSIX=1
)
