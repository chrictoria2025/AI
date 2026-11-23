#include "RTBManager.hpp"

#include <chrono>

namespace uav {

RTBManager::RTBManager() = default;

RTBManager::~RTBManager()
{
    sequenceRunning_ = false;
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
}

void RTBManager::onHealthUpdate(const HealthSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    // No state changes once RTB sequence is underway
    if (state_ == RTBState::RTB_INITIATED ||
        state_ == RTBState::RTB_IN_PROGRESS ||
        state_ == RTBState::LANDING ||
        state_ == RTBState::LANDED) {
        return;
    }

    TriggerResult trigger = evaluateTrigger(snapshot);

    switch (state_) {
    case RTBState::NOMINAL:
        if (trigger == TriggerResult::RTB) {
            transitionTo(RTBState::RTB_INITIATED);
            beginRTBSequence();
        } else if (trigger == TriggerResult::DEGRADE) {
            transitionTo(RTBState::DEGRADED);
        }
        break;

    case RTBState::DEGRADED:
        if (trigger == TriggerResult::RTB) {
            transitionTo(RTBState::RTB_INITIATED);
            beginRTBSequence();
        } else if (trigger == TriggerResult::NONE) {
            transitionTo(RTBState::NOMINAL);
        }
        break;

    default:
        break;
    }
}

RTBState RTBManager::getState() const
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

void RTBManager::registerStateCallback(RTBStateCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.push_back(std::move(callback));
}

void RTBManager::reset()
{
    sequenceRunning_ = false;
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }

    std::lock_guard<std::mutex> lock(stateMutex_);
    state_ = RTBState::NOMINAL;
}

RTBManager::TriggerResult RTBManager::evaluateTrigger(const HealthSnapshot& snapshot) const
{
    bool tempCritical = (snapshot.temperatureAlert == AlertLevel::CRITICAL);
    bool battCritical = (snapshot.batteryAlert == AlertLevel::CRITICAL);
    bool motorCritical = (snapshot.motorAlert == AlertLevel::CRITICAL);

    int criticalCount = (tempCritical ? 1 : 0)
                      + (battCritical ? 1 : 0)
                      + (motorCritical ? 1 : 0);

    // 2+ subsystems critical → RTB
    if (criticalCount >= 2) {
        return TriggerResult::RTB;
    }

    // CPU temperature critical alone → RTB (flight safety override)
    if (tempCritical) {
        return TriggerResult::RTB;
    }

    // Battery critical alone → RTB (power safety override)
    if (battCritical) {
        return TriggerResult::RTB;
    }

    // Motor critical alone → DEGRADED only (not RTB)
    if (motorCritical) {
        return TriggerResult::DEGRADE;
    }

    // Any WARNING → DEGRADED
    if (snapshot.temperatureAlert == AlertLevel::WARNING ||
        snapshot.batteryAlert == AlertLevel::WARNING ||
        snapshot.motorAlert == AlertLevel::WARNING) {
        return TriggerResult::DEGRADE;
    }

    return TriggerResult::NONE;
}

void RTBManager::transitionTo(RTBState newState)
{
    RTBState prev = state_;
    state_ = newState;

    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : callbacks_) {
        cb(prev, newState);
    }
}

void RTBManager::beginRTBSequence()
{
    if (sequenceThread_.joinable()) {
        sequenceThread_.join();
    }
    sequenceRunning_ = true;
    sequenceThread_ = std::thread(&RTBManager::rtbSequenceLoop, this);
}

void RTBManager::rtbSequenceLoop()
{
    // RTB_INITIATED → (2s) → RTB_IN_PROGRESS
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!sequenceRunning_) return;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        transitionTo(RTBState::RTB_IN_PROGRESS);
    }

    // RTB_IN_PROGRESS → (3s) → LANDING
    std::this_thread::sleep_for(std::chrono::seconds(3));
    if (!sequenceRunning_) return;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        transitionTo(RTBState::LANDING);
    }

    // LANDING → (2s) → LANDED
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!sequenceRunning_) return;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        transitionTo(RTBState::LANDED);
    }

    sequenceRunning_ = false;
}

} // namespace uav
