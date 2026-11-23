#include "PosixSensorHAL.hpp"
#include "HealthMonitor.hpp"
#include "RTBManager.hpp"
#include "MAVLinkReporter.hpp"
#include "EventLogger.hpp"
#include "FileFlashStorage.hpp"
#include "DashboardUI.hpp"
#include "ITransport.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <thread>

using namespace uav;

static std::atomic<bool> g_running{true};

static void signalHandler(int)
{
    g_running = false;
}

class DashboardTransport : public ITransport {
public:
    explicit DashboardTransport(DashboardUI& ui) : ui_(ui) {}

    void send(const uint8_t* data, size_t len) override
    {
        if (len == sizeof(MAVLinkHealthPayload)) {
            MAVLinkHealthPayload msg{};
            std::memcpy(&msg, data, sizeof(msg));

            char buf[128];
            const char* alertNames[] = {"NOMINAL", "WARNING", "CRITICAL"};
            const char* rtbNames[] = {"NOMINAL", "DEGRADED", "RTB_INITIATED",
                                       "RTB_IN_PROGRESS", "LANDING", "LANDED"};

            uint8_t ai = msg.alertLevel < 3 ? msg.alertLevel : 0;
            uint8_t ri = msg.rtbState < 6 ? msg.rtbState : 0;

            std::snprintf(buf, sizeof(buf),
                "[HTH] TEMP=%.1fC  VOLT=%.2fV  MOTOR=%.0f%%  ALERT=%s  RTB=%s",
                msg.temperature, msg.voltage, msg.motorHealth,
                alertNames[ai], rtbNames[ri]);
            ui_.addMAVLinkLog(buf);
        } else if (len == sizeof(MAVLinkStatusText)) {
            MAVLinkStatusText msg{};
            std::memcpy(&msg, data, sizeof(msg));

            char buf[128];
            std::snprintf(buf, sizeof(buf), "[TXT] %s", msg.text);
            ui_.addMAVLinkLog(buf);
        } else if (len == sizeof(MAVLinkHeartbeat)) {
            MAVLinkHeartbeat msg{};
            std::memcpy(&msg, data, sizeof(msg));

            char buf[64];
            std::snprintf(buf, sizeof(buf), "[HBT] system_status=%d", msg.system_status);
            ui_.addMAVLinkLog(buf);
        }
    }

private:
    DashboardUI& ui_;
};

int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // --- Initialize all units ---
    PosixSensorHAL hal;
    FileFlashStorage flash("flight_log.bin");
    EventLogger logger(flash);
    DashboardUI dashboard;
    DashboardTransport transport(dashboard);

    HealthMonitor monitor(hal, 100);
    RTBManager rtbManager;
    MAVLinkReporter reporter(transport);

    // --- Wire callbacks ---
    monitor.registerAlertCallback([&](AlertLevel prev, AlertLevel next, const HealthSnapshot& snapshot) {
        reporter.onAlertChange(prev, next, snapshot);

        EventType evType = EventType::NOMINAL;
        if (next == AlertLevel::WARNING) evType = EventType::WARNING;
        else if (next == AlertLevel::CRITICAL) evType = EventType::CRITICAL;

        char msg[64];
        std::snprintf(msg, sizeof(msg), "Alert: %s->%s T=%.1f V=%.2f M=%.0f",
            (prev == AlertLevel::NOMINAL ? "NOM" : prev == AlertLevel::WARNING ? "WRN" : "CRT"),
            (next == AlertLevel::NOMINAL ? "NOM" : next == AlertLevel::WARNING ? "WRN" : "CRT"),
            snapshot.readings.temperature, snapshot.readings.voltage,
            snapshot.readings.motorHealth);

        logger.logEvent(evType, SubsystemType::SYSTEM, 0.0f,
                        snapshot.timestamp_us, msg);
        dashboard.addEventLog(msg);
    });

    rtbManager.registerStateCallback([&](RTBState prev, RTBState next) {
        (void)prev;
        reporter.updateRTBState(next);

        EventType evType = EventType::NOMINAL;
        if (next == RTBState::RTB_INITIATED) evType = EventType::RTB_INITIATED;
        else if (next == RTBState::LANDED) evType = EventType::RTB_COMPLETE;
        else if (next == RTBState::DEGRADED) evType = EventType::DEGRADED_MODE;

        char msg[64];
        const char* names[] = {"NOMINAL", "DEGRADED", "RTB_INIT",
                               "RTB_PROG", "LANDING", "LANDED"};
        uint8_t ni = static_cast<uint8_t>(next);
        std::snprintf(msg, sizeof(msg), "RTB State -> %s", names[ni < 6 ? ni : 0]);

        logger.logEvent(evType, SubsystemType::SYSTEM, 0.0f,
                        hal.getTimestampMicros(), msg);
        dashboard.addEventLog(msg);
    });

    // --- Log startup ---
    logger.logEvent(EventType::STARTUP, SubsystemType::SYSTEM, 0.0f,
                    hal.getTimestampMicros(), "Health Guardian initialized");
    dashboard.addEventLog("System startup — Health Guardian initialized");

    // --- Start tasks ---
    monitor.start();
    reporter.start();

    // --- Main loop: update dashboard and feed RTB manager ---
    while (g_running) {
        auto snapshot = monitor.getLatestSnapshot();
        rtbManager.onHealthUpdate(snapshot);
        reporter.updateHealth(snapshot);

        dashboard.update(snapshot, rtbManager.getState());

        // Stop after LANDED
        if (rtbManager.getState() == RTBState::LANDED) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // --- Shutdown ---
    monitor.stop();
    reporter.stop();

    logger.logEvent(EventType::SHUTDOWN, SubsystemType::SYSTEM, 0.0f,
                    hal.getTimestampMicros(), "Health Guardian shutdown");
    logger.flush();

    // Final status
    std::printf("\n\n");
    std::printf("═══ SIMULATION COMPLETE ═══\n");
    std::printf("  Log entries: %u\n", logger.getEntryCount());
    uint32_t firstFail = 0;
    bool integrity = logger.verifyAllEntries(firstFail);
    std::printf("  Log integrity: %s\n", integrity ? "PASS" : "FAIL");
    if (!integrity) {
        std::printf("  First failure at index: %u\n", firstFail);
    }
    std::printf("  MAVLink messages sent: %u\n", reporter.getMessagesSent());
    std::printf("═══════════════════════════\n");

    return 0;
}
