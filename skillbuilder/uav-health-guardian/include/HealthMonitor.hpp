#pragma once

#include "ISensorHAL.hpp"
#include "SensorTypes.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace uav {

struct ThresholdConfig {
    float warningHigh;
    float criticalHigh;
    float warningLow;
    float criticalLow;
};

using AlertCallback = std::function<void(AlertLevel prev, AlertLevel next, const HealthSnapshot&)>;

class HealthMonitor {
public:
    explicit HealthMonitor(ISensorHAL& hal, uint32_t periodMs = 100);
    ~HealthMonitor();

    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;

    void start();
    void stop();

    HealthSnapshot getLatestSnapshot() const;
    void registerAlertCallback(AlertCallback callback);
    void evaluateOnce();

    void setTemperatureThresholds(float warning, float critical);
    void setBatteryThresholds(float warning, float critical);
    void setMotorThresholds(float warning, float critical);

private:
    AlertLevel evaluateTemperature(float value) const;
    AlertLevel evaluateBattery(float value) const;
    AlertLevel evaluateMotor(float value) const;
    void taskLoop();

    ISensorHAL& hal_;
    uint32_t periodMs_;

    mutable std::mutex snapshotMutex_;
    HealthSnapshot snapshot_{};
    uint32_t sequenceNumber_{0};

    std::mutex thresholdMutex_;
    ThresholdConfig tempThresholds_;
    ThresholdConfig batteryThresholds_;
    ThresholdConfig motorThresholds_;

    std::mutex callbackMutex_;
    std::vector<AlertCallback> callbacks_;

    std::atomic<bool> running_{false};
    std::thread taskThread_;
};

} // namespace uav
