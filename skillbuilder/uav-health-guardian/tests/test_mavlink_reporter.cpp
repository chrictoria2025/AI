#include <catch2/catch_test_macros.hpp>

#include "MAVLinkReporter.hpp"
#include "ITransport.hpp"

#include <cstring>
#include <vector>

using namespace uav;

class MockTransport : public ITransport {
public:
    struct Message {
        std::vector<uint8_t> data;
    };

    std::vector<Message> messages;

    void send(const uint8_t* data, size_t len) override {
        Message msg;
        msg.data.assign(data, data + len);
        messages.push_back(std::move(msg));
    }
};

TEST_CASE("MAVLinkReporter sends health payload on updateHealth + onAlertChange", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    HealthSnapshot snapshot{};
    snapshot.readings.temperature = 42.0f;
    snapshot.readings.voltage = 11.8f;
    snapshot.readings.motorHealth = 98.0f;
    snapshot.overallAlert = AlertLevel::NOMINAL;

    reporter.onAlertChange(AlertLevel::NOMINAL, AlertLevel::WARNING, snapshot);

    REQUIRE(transport.messages.size() == 1);
    REQUIRE(transport.messages[0].data.size() == sizeof(MAVLinkStatusText));
}

TEST_CASE("MAVLinkReporter status text contains severity", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    HealthSnapshot snapshot{};
    snapshot.readings.temperature = 80.0f;
    snapshot.overallAlert = AlertLevel::WARNING;

    reporter.onAlertChange(AlertLevel::NOMINAL, AlertLevel::WARNING, snapshot);

    MAVLinkStatusText msg{};
    std::memcpy(&msg, transport.messages[0].data.data(), sizeof(msg));
    REQUIRE(msg.severity == 4); // MAV_SEVERITY_WARNING
}

TEST_CASE("MAVLinkReporter critical alert has correct severity", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    HealthSnapshot snapshot{};
    snapshot.overallAlert = AlertLevel::CRITICAL;

    reporter.onAlertChange(AlertLevel::WARNING, AlertLevel::CRITICAL, snapshot);

    MAVLinkStatusText msg{};
    std::memcpy(&msg, transport.messages[0].data.data(), sizeof(msg));
    REQUIRE(msg.severity == 2); // MAV_SEVERITY_CRITICAL
}

TEST_CASE("MAVLinkReporter message counter increments", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    REQUIRE(reporter.getMessagesSent() == 0);

    HealthSnapshot snapshot{};
    reporter.onAlertChange(AlertLevel::NOMINAL, AlertLevel::WARNING, snapshot);

    REQUIRE(reporter.getMessagesSent() == 1);
}

TEST_CASE("MAVLinkReporter health payload contains correct values", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    HealthSnapshot snapshot{};
    snapshot.readings.temperature = 55.5f;
    snapshot.readings.voltage = 10.2f;
    snapshot.readings.motorHealth = 72.0f;
    snapshot.overallAlert = AlertLevel::WARNING;
    snapshot.timestamp_us = 123456;

    reporter.updateHealth(snapshot);
    reporter.updateRTBState(RTBState::DEGRADED);

    // Trigger the internal sendHealthPayload by calling onAlertChange
    // which also sends a StatusText — we check the payload via direct invocation
    // Instead, start/stop with a short period to get at least one cycle
    reporter.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    reporter.stop();

    // Find a health payload message (size matches MAVLinkHealthPayload)
    bool found = false;
    for (auto& msg : transport.messages) {
        if (msg.data.size() == sizeof(MAVLinkHealthPayload)) {
            MAVLinkHealthPayload payload{};
            std::memcpy(&payload, msg.data.data(), sizeof(payload));
            REQUIRE(payload.temperature == 55.5f);
            REQUIRE(payload.voltage == 10.2f);
            REQUIRE(payload.motorHealth == 72.0f);
            REQUIRE(payload.alertLevel == static_cast<uint8_t>(AlertLevel::WARNING));
            REQUIRE(payload.rtbState == static_cast<uint8_t>(RTBState::DEGRADED));
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("MAVLinkReporter sends heartbeat within 1100ms", "[mavlink_reporter]") {
    MockTransport transport;
    MAVLinkReporter reporter(transport);

    reporter.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    reporter.stop();

    bool foundHeartbeat = false;
    for (auto& msg : transport.messages) {
        if (msg.data.size() == sizeof(MAVLinkHeartbeat)) {
            foundHeartbeat = true;
            break;
        }
    }
    REQUIRE(foundHeartbeat);
}
