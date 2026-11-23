#include "MAVLinkReporter.hpp"

#include <chrono>
#include <cstring>

namespace uav {

MAVLinkReporter::MAVLinkReporter(ITransport& transport, uint8_t sysid, uint8_t compid)
    : transport_(transport)
    , sysid_(sysid)
    , compid_(compid)
{
}

MAVLinkReporter::~MAVLinkReporter()
{
    stop();
}

void MAVLinkReporter::start()
{
    if (running_.exchange(true)) {
        return;
    }
    taskThread_ = std::thread(&MAVLinkReporter::taskLoop, this);
}

void MAVLinkReporter::stop()
{
    running_ = false;
    if (taskThread_.joinable()) {
        taskThread_.join();
    }
}

void MAVLinkReporter::updateHealth(const HealthSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    latestSnapshot_ = snapshot;
}

void MAVLinkReporter::updateRTBState(RTBState state)
{
    std::lock_guard<std::mutex> lock(dataMutex_);
    rtbState_ = state;
}

void MAVLinkReporter::onAlertChange(AlertLevel prev, AlertLevel next, const HealthSnapshot& snapshot)
{
    (void)prev;
    std::lock_guard<std::mutex> lock(dataMutex_);
    latestSnapshot_ = snapshot;
    sendStatusText(next, snapshot);
}

uint32_t MAVLinkReporter::getMessagesSent() const
{
    return messagesSent_.load();
}

void MAVLinkReporter::taskLoop()
{
    while (running_) {
        heartbeatCounter_++;

        sendHealthPayload();
        sendSysStatus();

        // Heartbeat every 1000ms (every 2nd cycle at 500ms period)
        if (heartbeatCounter_ % 2 == 0) {
            sendHeartbeat();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void MAVLinkReporter::sendHeartbeat()
{
    MAVLinkHeartbeat msg{};
    msg.type = 2;           // MAV_TYPE_QUADROTOR
    msg.autopilot = 3;      // MAV_AUTOPILOT_ARDUPILOTMEGA
    msg.base_mode = 0x80;   // MAV_MODE_FLAG_SAFETY_ARMED
    msg.mavlink_version = 2;

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        switch (rtbState_) {
        case RTBState::NOMINAL:
        case RTBState::DEGRADED:
            msg.system_status = 4; // MAV_STATE_ACTIVE
            break;
        case RTBState::RTB_INITIATED:
        case RTBState::RTB_IN_PROGRESS:
            msg.system_status = 5; // MAV_STATE_CRITICAL
            break;
        case RTBState::LANDING:
        case RTBState::LANDED:
            msg.system_status = 6; // MAV_STATE_EMERGENCY / LANDED
            break;
        }
    }

    transport_.send(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    ++seq_;
    ++messagesSent_;
}

void MAVLinkReporter::sendSysStatus()
{
    MAVLinkSysStatus msg{};

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        msg.voltage_battery = static_cast<uint16_t>(latestSnapshot_.readings.voltage * 1000.0f);
        msg.battery_remaining = static_cast<int8_t>(
            ((latestSnapshot_.readings.voltage - 9.0f) / (12.6f - 9.0f)) * 100.0f);

        uint32_t health = 0;
        if (latestSnapshot_.temperatureAlert == AlertLevel::NOMINAL) health |= 0x01;
        if (latestSnapshot_.batteryAlert == AlertLevel::NOMINAL) health |= 0x02;
        if (latestSnapshot_.motorAlert == AlertLevel::NOMINAL) health |= 0x04;
        msg.onboard_control_sensors_health = health;
    }

    msg.onboard_control_sensors_present = 0x07;
    msg.onboard_control_sensors_enabled = 0x07;

    transport_.send(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    ++seq_;
    ++messagesSent_;
}

void MAVLinkReporter::sendHealthPayload()
{
    MAVLinkHealthPayload msg{};

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        msg.temperature = latestSnapshot_.readings.temperature;
        msg.voltage = latestSnapshot_.readings.voltage;
        msg.motorHealth = latestSnapshot_.readings.motorHealth;
        msg.alertLevel = static_cast<uint8_t>(latestSnapshot_.overallAlert);
        msg.rtbState = static_cast<uint8_t>(rtbState_);
        msg.timestamp_us = latestSnapshot_.timestamp_us;
    }

    transport_.send(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    ++seq_;
    ++messagesSent_;
}

void MAVLinkReporter::sendStatusText(AlertLevel level, const HealthSnapshot& snapshot)
{
    MAVLinkStatusText msg{};

    switch (level) {
    case AlertLevel::NOMINAL:
        msg.severity = 6; // MAV_SEVERITY_INFO
        std::snprintf(msg.text, sizeof(msg.text), "All subsystems NOMINAL");
        break;
    case AlertLevel::WARNING:
        msg.severity = 4; // MAV_SEVERITY_WARNING
        std::snprintf(msg.text, sizeof(msg.text),
            "WARNING T=%.1fC V=%.2fV M=%.0f%%",
            snapshot.readings.temperature,
            snapshot.readings.voltage,
            snapshot.readings.motorHealth);
        break;
    case AlertLevel::CRITICAL:
        msg.severity = 2; // MAV_SEVERITY_CRITICAL
        std::snprintf(msg.text, sizeof(msg.text),
            "CRITICAL T=%.1fC V=%.2fV M=%.0f%%",
            snapshot.readings.temperature,
            snapshot.readings.voltage,
            snapshot.readings.motorHealth);
        break;
    }

    transport_.send(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    ++seq_;
    ++messagesSent_;
}

} // namespace uav
