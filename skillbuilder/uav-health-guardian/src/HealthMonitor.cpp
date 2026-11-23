#include "HealthMonitor.hpp"

#include <algorithm>
#include <chrono>

namespace uav {

HealthMonitor::HealthMonitor(ISensorHAL& hal, uint32_t periodMs)
    : hal_(hal)
    , periodMs_(periodMs)
    , tempThresholds_{75.0f, 95.0f, 0.0f, 0.0f}
    , batteryThresholds_{0.0f, 0.0f, 10.5f, 9.5f}
    , motorThresholds_{0.0f, 0.0f, 70.0f, 50.0f}
{
}

HealthMonitor::~HealthMonitor()
{
    stop();
}

void HealthMonitor::start()
{
    if (running_.exchange(true)) {
        return;
    }
    taskThread_ = std::thread(&HealthMonitor::taskLoop, this);
}

void HealthMonitor::stop()
{
    running_ = false;
    if (taskThread_.joinable()) {
        taskThread_.join();
    }
}

HealthSnapshot HealthMonitor::getLatestSnapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex_);
    return snapshot_;
}

void HealthMonitor::registerAlertCallback(AlertCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.push_back(std::move(callback));
}

void HealthMonitor::evaluateOnce()
{
    SensorReading reading{};
    reading.temperature = hal_.readCPUTemperature();
    reading.voltage = hal_.readBatteryVoltage();
    reading.motorHealth = hal_.readMotorControllerHealth();
    reading.timestamp_us = hal_.getTimestampMicros();

    AlertLevel tempAlert;
    AlertLevel battAlert;
    AlertLevel motorAlert;

    {
        std::lock_guard<std::mutex> lock(thresholdMutex_);
        tempAlert = evaluateTemperature(reading.temperature);
        battAlert = evaluateBattery(reading.voltage);
        motorAlert = evaluateMotor(reading.motorHealth);
    }

    AlertLevel overall = std::max({tempAlert, battAlert, motorAlert});

    AlertLevel previousOverall;
    HealthSnapshot newSnapshot{};

    {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        previousOverall = snapshot_.overallAlert;

        newSnapshot.readings = reading;
        newSnapshot.temperatureAlert = tempAlert;
        newSnapshot.batteryAlert = battAlert;
        newSnapshot.motorAlert = motorAlert;
        newSnapshot.overallAlert = overall;
        newSnapshot.timestamp_us = reading.timestamp_us;
        newSnapshot.sequenceNumber = ++sequenceNumber_;

        snapshot_ = newSnapshot;
    }

    if (overall != previousOverall) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        for (auto& cb : callbacks_) {
            cb(previousOverall, overall, newSnapshot);
        }
    }
}

void HealthMonitor::setTemperatureThresholds(float warning, float critical)
{
    std::lock_guard<std::mutex> lock(thresholdMutex_);
    tempThresholds_.warningHigh = warning;
    tempThresholds_.criticalHigh = critical;
}

void HealthMonitor::setBatteryThresholds(float warning, float critical)
{
    std::lock_guard<std::mutex> lock(thresholdMutex_);
    batteryThresholds_.warningLow = warning;
    batteryThresholds_.criticalLow = critical;
}

void HealthMonitor::setMotorThresholds(float warning, float critical)
{
    std::lock_guard<std::mutex> lock(thresholdMutex_);
    motorThresholds_.warningLow = warning;
    motorThresholds_.criticalLow = critical;
}

AlertLevel HealthMonitor::evaluateTemperature(float value) const
{
    if (value > tempThresholds_.criticalHigh) {
        return AlertLevel::CRITICAL;
    }
    if (value >= tempThresholds_.warningHigh) {
        return AlertLevel::WARNING;
    }
    return AlertLevel::NOMINAL;
}

AlertLevel HealthMonitor::evaluateBattery(float value) const
{
    if (value <= batteryThresholds_.criticalLow) {
        return AlertLevel::CRITICAL;
    }
    if (value <= batteryThresholds_.warningLow) {
        return AlertLevel::WARNING;
    }
    return AlertLevel::NOMINAL;
}

AlertLevel HealthMonitor::evaluateMotor(float value) const
{
    if (value <= motorThresholds_.criticalLow) {
        return AlertLevel::CRITICAL;
    }
    if (value <= motorThresholds_.warningLow) {
        return AlertLevel::WARNING;
    }
    return AlertLevel::NOMINAL;
}

void HealthMonitor::taskLoop()
{
    while (running_) {
        evaluateOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(periodMs_));
    }
}

} // namespace uav
