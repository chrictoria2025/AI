#pragma once

#include <cstdint>

namespace uav {

class ISensorHAL {
public:
    virtual ~ISensorHAL() = default;

    virtual float readCPUTemperature() = 0;
    virtual float readBatteryVoltage() = 0;
    virtual float readMotorControllerHealth() = 0;
    virtual uint32_t getTimestampMicros() = 0;
    virtual bool isHealthy() = 0;
};

} // namespace uav
