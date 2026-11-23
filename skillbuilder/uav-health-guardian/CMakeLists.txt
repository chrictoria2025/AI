cmake_minimum_required(VERSION 3.14)
project(uav-health-guardian LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_TARGET "posix" CACHE STRING "Build target: posix, pico, esp32s3")
set_property(CACHE BUILD_TARGET PROPERTY STRINGS posix pico esp32s3)

message(STATUS "Build target: ${BUILD_TARGET}")

find_package(Threads REQUIRED)

# --- Core library (all targets) ---
add_library(uav_core STATIC
    src/hal/posix/PosixSensorHAL.cpp
    src/HealthMonitor.cpp
    src/RTBManager.cpp
    src/MAVLinkReporter.cpp
    src/EventLogger.cpp
)
target_include_directories(uav_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/hal/posix
)
target_link_libraries(uav_core PUBLIC Threads::Threads)

# --- Target-specific configuration ---
if(BUILD_TARGET STREQUAL "posix")
    message(STATUS "Configuring for POSIX (macOS/Linux/Windows development)")
    target_compile_definitions(uav_core PUBLIC UAV_TARGET_POSIX=1)

    add_executable(uav_health_guardian_posix
        src/main.cpp
        src/DashboardUI.cpp
    )
    target_link_libraries(uav_health_guardian_posix PRIVATE uav_core)

elseif(BUILD_TARGET STREQUAL "pico")
    message(STATUS "Configuring for Raspberry Pi Pico W (placeholder)")
    target_compile_definitions(uav_core PUBLIC UAV_TARGET_PICO=1)
    message(WARNING "Pico target is a placeholder — requires pico-sdk to build fully")

elseif(BUILD_TARGET STREQUAL "esp32s3")
    message(STATUS "Configuring for ESP32-S3 via ESP-IDF (placeholder)")
    target_compile_definitions(uav_core PUBLIC UAV_TARGET_ESP32S3=1)
    message(WARNING "ESP32-S3 target is a placeholder — requires ESP-IDF to build fully")

else()
    message(FATAL_ERROR "Unknown BUILD_TARGET: ${BUILD_TARGET}. Use posix, pico, or esp32s3.")
endif()

# --- Tests (posix only) ---
if(BUILD_TARGET STREQUAL "posix")
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    set(TEST_SOURCES
        tests/test_sensor_hal.cpp
        tests/test_health_monitor.cpp
        tests/test_rtb_manager.cpp
        tests/test_mavlink_reporter.cpp
        tests/test_event_logger.cpp
    )

    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE uav_core Catch2::Catch2WithMain)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
