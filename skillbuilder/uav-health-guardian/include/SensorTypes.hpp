#pragma once

#include <cstdint>

namespace uav {

enum class AlertLevel : uint8_t {
    NOMINAL  = 0,
    WARNING  = 1,
    CRITICAL = 2
};

enum class SubsystemType : uint8_t {
    CPU_TEMP        = 0,
    BATTERY_VOLTAGE = 1,
    MOTOR_HEALTH    = 2,
    SYSTEM          = 3
};

enum class RTBState : uint8_t {
    NOMINAL         = 0,
    DEGRADED        = 1,
    RTB_INITIATED   = 2,
    RTB_IN_PROGRESS = 3,
    LANDING         = 4,
    LANDED          = 5
};

enum class EventType : uint8_t {
    STARTUP        = 0,
    NOMINAL        = 1,
    WARNING        = 2,
    CRITICAL       = 3,
    RTB_INITIATED  = 4,
    RTB_COMPLETE   = 5,
    DEGRADED_MODE  = 6,
    WATCHDOG_RESET = 7,
    SHUTDOWN       = 8
};

struct SensorReading {
    float temperature;
    float voltage;
    float motorHealth;
    uint64_t timestamp_us;
};

struct HealthSnapshot {
    SensorReading readings;

    AlertLevel temperatureAlert;
    AlertLevel batteryAlert;
    AlertLevel motorAlert;
    AlertLevel overallAlert;

    uint64_t timestamp_us;
    uint32_t sequenceNumber;
};

struct LogEntry {
    EventType eventType;
    SubsystemType subsystem;
    uint8_t _reserved[2];
    float sensorValue;
    uint64_t timestamp_us;
    uint32_t sequenceNumber;
    uint32_t crc32;
    char message[64];
};

} // namespace uav
