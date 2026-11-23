#include <catch2/catch_test_macros.hpp>

#include "RTBManager.hpp"

using namespace uav;

static HealthSnapshot makeSnapshot(AlertLevel temp, AlertLevel batt, AlertLevel motor) {
    HealthSnapshot s{};
    s.temperatureAlert = temp;
    s.batteryAlert = batt;
    s.motorAlert = motor;
    s.overallAlert = std::max({temp, batt, motor});
    return s;
}

TEST_CASE("RTBManager starts in NOMINAL state", "[rtb_manager]") {
    RTBManager mgr;
    REQUIRE(mgr.getState() == RTBState::NOMINAL);
}

TEST_CASE("RTBManager WARNING enters DEGRADED mode", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::WARNING, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::DEGRADED);
}

TEST_CASE("RTBManager DEGRADED returns to NOMINAL on all-clear", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::WARNING, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::DEGRADED);

    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::NOMINAL);
}

TEST_CASE("RTBManager critical temperature triggers RTB", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::CRITICAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);
}

TEST_CASE("RTBManager critical battery triggers RTB", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::CRITICAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);
}

TEST_CASE("RTBManager critical motor alone enters DEGRADED not RTB", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::NOMINAL, AlertLevel::CRITICAL));
    REQUIRE(mgr.getState() == RTBState::DEGRADED);
}

TEST_CASE("RTBManager two critical subsystems triggers RTB", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::CRITICAL, AlertLevel::CRITICAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);
}

TEST_CASE("RTBManager DEGRADED to RTB on critical escalation", "[rtb_manager]") {
    RTBManager mgr;
    // Enter degraded via motor warning
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::NOMINAL, AlertLevel::WARNING));
    REQUIRE(mgr.getState() == RTBState::DEGRADED);

    // Escalate via temperature critical
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::CRITICAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);
}

TEST_CASE("RTBManager state callback fires on transition", "[rtb_manager]") {
    RTBManager mgr;

    RTBState capturedPrev = RTBState::LANDED;
    RTBState capturedNext = RTBState::LANDED;
    int callCount = 0;

    mgr.registerStateCallback([&](RTBState prev, RTBState next) {
        capturedPrev = prev;
        capturedNext = next;
        ++callCount;
    });

    mgr.onHealthUpdate(makeSnapshot(AlertLevel::WARNING, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(callCount == 1);
    REQUIRE(capturedPrev == RTBState::NOMINAL);
    REQUIRE(capturedNext == RTBState::DEGRADED);
}

TEST_CASE("RTBManager ignores updates once RTB initiated", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::CRITICAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);

    // Try to return to nominal — should be ignored
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::NOMINAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);
}

TEST_CASE("RTBManager reset returns to NOMINAL", "[rtb_manager]") {
    RTBManager mgr;
    mgr.onHealthUpdate(makeSnapshot(AlertLevel::CRITICAL, AlertLevel::NOMINAL, AlertLevel::NOMINAL));
    REQUIRE(mgr.getState() == RTBState::RTB_INITIATED);

    mgr.reset();
    REQUIRE(mgr.getState() == RTBState::NOMINAL);
}
