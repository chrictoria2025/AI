#include "PosixSensorHAL.hpp"

namespace uav {

PosixSensorHAL::PosixSensorHAL()
    : start_time_(std::chrono::steady_clock::now())
    , rng_(std::random_device{}())
    , noise_(0.0f, 1.0f)
    , healthy_(true)
{
}

float PosixSensorHAL::readCPUTemperature()
{
    float elapsed = getElapsedSeconds();
    float base = 40.0f + (elapsed * 0.3f);
    float jitter = noise_(rng_) * 0.5f;

    // After ~15 seconds, inject a critical temperature spike for demo
    if (elapsed > 15.0f) {
        base = 97.0f + (elapsed - 15.0f) * 2.0f;
    }

    return base + jitter;
}

float PosixSensorHAL::readBatteryVoltage()
{
    float elapsed = getElapsedSeconds();
    // 3S LiPo: starts at 11.8V, depletes at ~0.02V/s for demo
    float base = 11.8f - (elapsed * 0.02f);
    float jitter = noise_(rng_) * 0.05f;

    if (base < 9.0f) {
        base = 9.0f;
    }

    return base + jitter;
}

float PosixSensorHAL::readMotorControllerHealth()
{
    float elapsed = getElapsedSeconds();
    // Starts at 100%, degrades slowly at ~0.5%/s
    float base = 100.0f - (elapsed * 0.5f);
    float jitter = noise_(rng_) * 0.3f;

    if (base < 0.0f) {
        base = 0.0f;
    }

    return base + jitter;
}

uint32_t PosixSensorHAL::getTimestampMicros()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        now - start_time_);
    return static_cast<uint32_t>(duration.count());
}

bool PosixSensorHAL::isHealthy()
{
    return healthy_;
}

float PosixSensorHAL::getElapsedSeconds() const
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_);
    return static_cast<float>(duration.count()) / 1000.0f;
}

} // namespace uav
