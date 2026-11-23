#include <catch2/catch_test_macros.hpp>

#include "HealthMonitor.hpp"
#include "ISensorHAL.hpp"

using namespace uav;

class MockSensorHAL : public ISensorHAL {
public:
    float temperature = 40.0f;
    float voltage = 11.8f;
    float motorHealth = 100.0f;
    uint32_t timestamp = 0;

    float readCPUTemperature() override { return temperature; }
    float readBatteryVoltage() override { return voltage; }
    float readMotorControllerHealth() override { return motorHealth; }
    uint32_t getTimestampMicros() override { return timestamp += 100000; }
    bool isHealthy() override { return true; }
};

TEST_CASE("HealthMonitor nominal readings produce NOMINAL alert", "[health_monitor]") {
    MockSensorHAL hal;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.temperatureAlert == AlertLevel::NOMINAL);
    REQUIRE(snapshot.batteryAlert == AlertLevel::NOMINAL);
    REQUIRE(snapshot.motorAlert == AlertLevel::NOMINAL);
    REQUIRE(snapshot.overallAlert == AlertLevel::NOMINAL);
}

TEST_CASE("HealthMonitor detects temperature WARNING", "[health_monitor]") {
    MockSensorHAL hal;
    hal.temperature = 80.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.temperatureAlert == AlertLevel::WARNING);
    REQUIRE(snapshot.overallAlert == AlertLevel::WARNING);
}

TEST_CASE("HealthMonitor detects temperature CRITICAL", "[health_monitor]") {
    MockSensorHAL hal;
    hal.temperature = 96.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.temperatureAlert == AlertLevel::CRITICAL);
    REQUIRE(snapshot.overallAlert == AlertLevel::CRITICAL);
}

TEST_CASE("HealthMonitor detects battery WARNING", "[health_monitor]") {
    MockSensorHAL hal;
    hal.voltage = 10.5f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.batteryAlert == AlertLevel::WARNING);
}

TEST_CASE("HealthMonitor detects battery CRITICAL", "[health_monitor]") {
    MockSensorHAL hal;
    hal.voltage = 9.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.batteryAlert == AlertLevel::CRITICAL);
}

TEST_CASE("HealthMonitor detects motor WARNING", "[health_monitor]") {
    MockSensorHAL hal;
    hal.motorHealth = 65.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.motorAlert == AlertLevel::WARNING);
}

TEST_CASE("HealthMonitor detects motor CRITICAL", "[health_monitor]") {
    MockSensorHAL hal;
    hal.motorHealth = 45.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.motorAlert == AlertLevel::CRITICAL);
}

TEST_CASE("HealthMonitor overall alert is max of all subsystems", "[health_monitor]") {
    MockSensorHAL hal;
    hal.temperature = 80.0f;  // WARNING
    hal.voltage = 9.0f;       // CRITICAL
    hal.motorHealth = 100.0f; // NOMINAL
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto snapshot = monitor.getLatestSnapshot();

    REQUIRE(snapshot.overallAlert == AlertLevel::CRITICAL);
}

TEST_CASE("HealthMonitor alert callback fires on level change", "[health_monitor]") {
    MockSensorHAL hal;
    HealthMonitor monitor(hal);

    AlertLevel capturedPrev = AlertLevel::CRITICAL;
    AlertLevel capturedNext = AlertLevel::CRITICAL;
    int callCount = 0;

    monitor.registerAlertCallback([&](AlertLevel prev, AlertLevel next, const HealthSnapshot&) {
        capturedPrev = prev;
        capturedNext = next;
        ++callCount;
    });

    // First call: NOMINAL (initial state is NOMINAL, snapshot default is NOMINAL — no change)
    monitor.evaluateOnce();
    REQUIRE(callCount == 0);

    // Trigger WARNING
    hal.temperature = 80.0f;
    monitor.evaluateOnce();
    REQUIRE(callCount == 1);
    REQUIRE(capturedPrev == AlertLevel::NOMINAL);
    REQUIRE(capturedNext == AlertLevel::WARNING);

    // Same WARNING — no callback
    monitor.evaluateOnce();
    REQUIRE(callCount == 1);
}

TEST_CASE("HealthMonitor runtime threshold update takes effect", "[health_monitor]") {
    MockSensorHAL hal;
    hal.temperature = 60.0f;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    REQUIRE(monitor.getLatestSnapshot().temperatureAlert == AlertLevel::NOMINAL);

    // Lower warning threshold so 60C is now a warning
    monitor.setTemperatureThresholds(55.0f, 95.0f);
    monitor.evaluateOnce();
    REQUIRE(monitor.getLatestSnapshot().temperatureAlert == AlertLevel::WARNING);
}

TEST_CASE("HealthMonitor sequence number increments", "[health_monitor]") {
    MockSensorHAL hal;
    HealthMonitor monitor(hal);

    monitor.evaluateOnce();
    auto s1 = monitor.getLatestSnapshot();
    monitor.evaluateOnce();
    auto s2 = monitor.getLatestSnapshot();

    REQUIRE(s2.sequenceNumber == s1.sequenceNumber + 1);
}
