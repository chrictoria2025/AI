#pragma once

#include "SensorTypes.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace uav {

using RTBStateCallback = std::function<void(RTBState prev, RTBState next)>;

class RTBManager {
public:
    RTBManager();
    ~RTBManager();

    RTBManager(const RTBManager&) = delete;
    RTBManager& operator=(const RTBManager&) = delete;

    void onHealthUpdate(const HealthSnapshot& snapshot);
    RTBState getState() const;
    void registerStateCallback(RTBStateCallback callback);
    void reset();

private:
    enum class TriggerResult : uint8_t {
        NONE,
        DEGRADE,
        RTB
    };

    TriggerResult evaluateTrigger(const HealthSnapshot& snapshot) const;
    void transitionTo(RTBState newState);
    void beginRTBSequence();
    void rtbSequenceLoop();

    mutable std::mutex stateMutex_;
    RTBState state_{RTBState::NOMINAL};

    std::mutex callbackMutex_;
    std::vector<RTBStateCallback> callbacks_;

    std::atomic<bool> sequenceRunning_{false};
    std::thread sequenceThread_;
};

} // namespace uav
