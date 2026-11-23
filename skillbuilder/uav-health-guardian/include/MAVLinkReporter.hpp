#pragma once

#include "ITransport.hpp"
#include "SensorTypes.hpp"

#include <atomic>
#include <mutex>
#include <thread>

namespace uav {

struct MAVLinkHeader {
    uint8_t magic;
    uint8_t len;
    uint8_t seq;
    uint8_t sysid;
    uint8_t compid;
    uint8_t msgid;
};

struct MAVLinkHeartbeat {
    static constexpr uint8_t MSG_ID = 0;
    uint32_t custom_mode;
    uint8_t type;
    uint8_t autopilot;
    uint8_t base_mode;
    uint8_t system_status;
    uint8_t mavlink_version;
};

struct MAVLinkSysStatus {
    static constexpr uint8_t MSG_ID = 1;
    uint32_t onboard_control_sensors_present;
    uint32_t onboard_control_sensors_enabled;
    uint32_t onboard_control_sensors_health;
    uint16_t load;
    uint16_t voltage_battery;
    int16_t current_battery;
    int8_t battery_remaining;
};

struct MAVLinkStatusText {
    static constexpr uint8_t MSG_ID = 253;
    uint8_t severity;
    char text[50];
};

struct MAVLinkHealthPayload {
    static constexpr uint8_t MSG_ID = 200;
    float temperature;
    float voltage;
    float motorHealth;
    uint8_t alertLevel;
    uint8_t rtbState;
    uint64_t timestamp_us;
};

class MAVLinkReporter {
public:
    MAVLinkReporter(ITransport& transport, uint8_t sysid = 1, uint8_t compid = 1);
    ~MAVLinkReporter();

    MAVLinkReporter(const MAVLinkReporter&) = delete;
    MAVLinkReporter& operator=(const MAVLinkReporter&) = delete;

    void start();
    void stop();

    void updateHealth(const HealthSnapshot& snapshot);
    void updateRTBState(RTBState state);
    void onAlertChange(AlertLevel prev, AlertLevel next, const HealthSnapshot& snapshot);

    uint32_t getMessagesSent() const;

private:
    void taskLoop();
    void sendHeartbeat();
    void sendSysStatus();
    void sendHealthPayload();
    void sendStatusText(AlertLevel level, const HealthSnapshot& snapshot);

    ITransport& transport_;
    uint8_t sysid_;
    uint8_t compid_;
    uint8_t seq_{0};

    mutable std::mutex dataMutex_;
    HealthSnapshot latestSnapshot_{};
    RTBState rtbState_{RTBState::NOMINAL};

    std::atomic<uint32_t> messagesSent_{0};
    std::atomic<bool> running_{false};
    std::thread taskThread_;

    uint32_t heartbeatCounter_{0};
};

} // namespace uav
