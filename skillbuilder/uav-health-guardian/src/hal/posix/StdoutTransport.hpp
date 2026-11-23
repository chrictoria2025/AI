#pragma once

#include "ITransport.hpp"
#include "MAVLinkReporter.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace uav {

class StdoutTransport : public ITransport {
public:
    StdoutTransport()
        : start_time_(std::chrono::steady_clock::now())
    {
    }

    void send(const uint8_t* data, size_t len) override
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_).count();

        if (len == sizeof(MAVLinkHealthPayload)) {
            MAVLinkHealthPayload msg{};
            std::memcpy(&msg, data, sizeof(msg));
            printHealth(elapsed, msg);
        } else if (len == sizeof(MAVLinkStatusText)) {
            MAVLinkStatusText msg{};
            std::memcpy(&msg, data, sizeof(msg));
            printStatusText(elapsed, msg);
        } else if (len == sizeof(MAVLinkHeartbeat)) {
            MAVLinkHeartbeat msg{};
            std::memcpy(&msg, data, sizeof(msg));
            printHeartbeat(elapsed, msg);
        } else if (len == sizeof(MAVLinkSysStatus)) {
            // SYS_STATUS is verbose; skip printing to keep output readable
        }
    }

private:
    void printHealth(long ms, const MAVLinkHealthPayload& msg)
    {
        const char* alertStr = alertToString(static_cast<AlertLevel>(msg.alertLevel));
        const char* rtbStr = rtbToString(static_cast<RTBState>(msg.rtbState));

        std::printf("[%6ldms] [MAVLINK HTH] TEMP=%.1fC  VOLT=%.2fV  MOTOR=%.0f%%  ALERT=%s  RTB=%s\n",
            ms, msg.temperature, msg.voltage, msg.motorHealth, alertStr, rtbStr);
    }

    void printStatusText(long ms, const MAVLinkStatusText& msg)
    {
        const char* sevStr = "INFO";
        if (msg.severity <= 2) sevStr = "CRITICAL";
        else if (msg.severity <= 4) sevStr = "WARNING";

        std::printf("[%6ldms] [MAVLINK TXT] [%s] %s\n", ms, sevStr, msg.text);
    }

    void printHeartbeat(long ms, const MAVLinkHeartbeat& msg)
    {
        const char* statusStr = "UNKNOWN";
        switch (msg.system_status) {
        case 4: statusStr = "ACTIVE"; break;
        case 5: statusStr = "CRITICAL"; break;
        case 6: statusStr = "EMERGENCY"; break;
        }
        std::printf("[%6ldms] [MAVLINK HBT] system_status=%s\n", ms, statusStr);
    }

    static const char* alertToString(AlertLevel level)
    {
        switch (level) {
        case AlertLevel::NOMINAL:  return "NOMINAL";
        case AlertLevel::WARNING:  return "WARNING";
        case AlertLevel::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    static const char* rtbToString(RTBState state)
    {
        switch (state) {
        case RTBState::NOMINAL:         return "NOMINAL";
        case RTBState::DEGRADED:        return "DEGRADED";
        case RTBState::RTB_INITIATED:   return "RTB_INITIATED";
        case RTBState::RTB_IN_PROGRESS: return "RTB_IN_PROGRESS";
        case RTBState::LANDING:         return "LANDING";
        case RTBState::LANDED:          return "LANDED";
        }
        return "UNKNOWN";
    }

    std::chrono::steady_clock::time_point start_time_;
};

} // namespace uav
