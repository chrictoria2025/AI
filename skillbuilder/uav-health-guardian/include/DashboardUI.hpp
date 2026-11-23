#pragma once

#include "SensorTypes.hpp"

#include <deque>
#include <mutex>
#include <string>

namespace uav {

class DashboardUI {
public:
    DashboardUI();

    void update(const HealthSnapshot& snapshot, RTBState rtbState);
    void addMAVLinkLog(const std::string& line);
    void addEventLog(const std::string& line);
    void render();

private:
    void moveCursor(int row, int col);
    void clearScreen();
    void setColor(int code);
    void resetColor();

    void renderHeader();
    void renderSensorPanel(const HealthSnapshot& snapshot);
    void renderRTBBanner(RTBState state);
    void renderMAVLinkPanel();
    void renderEventPanel();
    void renderProgressBar(float value, float maxVal, int width, int colorCode);

    const char* alertStr(AlertLevel level);
    const char* rtbStr(RTBState state);
    int alertColor(AlertLevel level);
    int rtbColor(RTBState state);

    std::mutex logMutex_;
    std::deque<std::string> mavlinkLog_;
    std::deque<std::string> eventLog_;

    static constexpr size_t MAX_LOG_LINES = 8;
    uint32_t frameCount_{0};
};

} // namespace uav
