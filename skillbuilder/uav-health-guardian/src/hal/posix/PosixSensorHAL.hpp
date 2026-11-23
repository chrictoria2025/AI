#pragma once

#include "ISensorHAL.hpp"
#include <chrono>
#include <random>

namespace uav {

class PosixSensorHAL : public ISensorHAL {
public:
    PosixSensorHAL();

    float readCPUTemperature() override;
    float readBatteryVoltage() override;
    float readMotorControllerHealth() override;
    uint32_t getTimestampMicros() override;
    bool isHealthy() override;

private:
    float getElapsedSeconds() const;

    std::chrono::steady_clock::time_point start_time_;
    std::mt19937 rng_;
    std::normal_distribution<float> noise_;
    bool healthy_;
};

} // namespace uav
