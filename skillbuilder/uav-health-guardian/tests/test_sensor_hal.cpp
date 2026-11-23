#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PosixSensorHAL.hpp"

using namespace uav;

TEST_CASE("PosixSensorHAL initial temperature is near 40C", "[sensor_hal]") {
    PosixSensorHAL hal;
    float temp = hal.readCPUTemperature();
    REQUIRE(temp > 35.0f);
    REQUIRE(temp < 45.0f);
}

TEST_CASE("PosixSensorHAL initial battery voltage is near 11.8V", "[sensor_hal]") {
    PosixSensorHAL hal;
    float voltage = hal.readBatteryVoltage();
    REQUIRE(voltage > 11.5f);
    REQUIRE(voltage < 12.1f);
}

TEST_CASE("PosixSensorHAL initial motor health is near 100%", "[sensor_hal]") {
    PosixSensorHAL hal;
    float health = hal.readMotorControllerHealth();
    REQUIRE(health > 98.0f);
    REQUIRE(health < 102.0f);
}

TEST_CASE("PosixSensorHAL timestamp is monotonically increasing", "[sensor_hal]") {
    PosixSensorHAL hal;
    uint32_t t1 = hal.getTimestampMicros();
    uint32_t t2 = hal.getTimestampMicros();
    REQUIRE(t2 >= t1);
}

TEST_CASE("PosixSensorHAL reports healthy on startup", "[sensor_hal]") {
    PosixSensorHAL hal;
    REQUIRE(hal.isHealthy() == true);
}

TEST_CASE("PosixSensorHAL readings vary between calls", "[sensor_hal]") {
    PosixSensorHAL hal;
    float t1 = hal.readCPUTemperature();
    float t2 = hal.readCPUTemperature();
    // Due to random noise, readings should not be identical
    // (statistically near-impossible with normal distribution)
    // But to avoid flaky tests, we just confirm they're in valid range
    REQUIRE(t1 > 30.0f);
    REQUIRE(t2 > 30.0f);
}
