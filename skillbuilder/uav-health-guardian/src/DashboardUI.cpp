#include "DashboardUI.hpp"

#include <cstdio>
#include <cstring>

namespace uav {

DashboardUI::DashboardUI() = default;

void DashboardUI::update(const HealthSnapshot& snapshot, RTBState rtbState)
{
    render();
    renderHeader();
    renderSensorPanel(snapshot);
    renderRTBBanner(rtbState);
    renderMAVLinkPanel();
    renderEventPanel();

    // Move cursor below the dashboard
    moveCursor(32, 1);
    std::fflush(stdout);
    ++frameCount_;
}

void DashboardUI::addMAVLinkLog(const std::string& line)
{
    std::lock_guard<std::mutex> lock(logMutex_);
    mavlinkLog_.push_back(line);
    if (mavlinkLog_.size() > MAX_LOG_LINES) {
        mavlinkLog_.pop_front();
    }
}

void DashboardUI::addEventLog(const std::string& line)
{
    std::lock_guard<std::mutex> lock(logMutex_);
    eventLog_.push_back(line);
    if (eventLog_.size() > MAX_LOG_LINES) {
        eventLog_.pop_front();
    }
}

void DashboardUI::render()
{
    clearScreen();
    moveCursor(1, 1);
}

void DashboardUI::moveCursor(int row, int col)
{
    std::printf("\033[%d;%dH", row, col);
}

void DashboardUI::clearScreen()
{
    std::printf("\033[2J");
}

void DashboardUI::setColor(int code)
{
    std::printf("\033[%dm", code);
}

void DashboardUI::resetColor()
{
    std::printf("\033[0m");
}

void DashboardUI::renderHeader()
{
    moveCursor(1, 1);
    setColor(1); // bold
    setColor(36); // cyan
    std::printf("╔══════════════════════════════════════════════════════════════════════════╗\n");
    std::printf("║          UAV FLIGHT COMPUTER HEALTH GUARDIAN — LIVE TELEMETRY           ║\n");
    std::printf("╚══════════════════════════════════════════════════════════════════════════╝");
    resetColor();
}

void DashboardUI::renderSensorPanel(const HealthSnapshot& snapshot)
{
    moveCursor(5, 1);
    setColor(1);
    std::printf("┌─── CPU TEMPERATURE ──┐  ┌─── BATTERY VOLTAGE ──┐  ┌── MOTOR HEALTH ────┐");
    resetColor();

    // CPU Temperature
    moveCursor(6, 1);
    std::printf("│ ");
    renderProgressBar(snapshot.readings.temperature, 120.0f, 20, alertColor(snapshot.temperatureAlert));
    std::printf(" │  ");

    // Battery Voltage
    std::printf("│ ");
    renderProgressBar(snapshot.readings.voltage, 12.6f, 20, alertColor(snapshot.batteryAlert));
    std::printf(" │  ");

    // Motor Health
    std::printf("│ ");
    renderProgressBar(snapshot.readings.motorHealth, 100.0f, 20, alertColor(snapshot.motorAlert));
    std::printf(" │");

    // Values row
    moveCursor(7, 1);
    setColor(alertColor(snapshot.temperatureAlert));
    std::printf("│  %6.1f °C            ", snapshot.readings.temperature);
    resetColor();
    std::printf("│  ");
    setColor(alertColor(snapshot.batteryAlert));
    std::printf("│  %5.2f V             ", snapshot.readings.voltage);
    resetColor();
    std::printf("│  ");
    setColor(alertColor(snapshot.motorAlert));
    std::printf("│  %5.1f %%             ", snapshot.readings.motorHealth);
    resetColor();
    std::printf("│");

    // Alert badges
    moveCursor(8, 1);
    std::printf("│  [");
    setColor(alertColor(snapshot.temperatureAlert));
    setColor(1);
    std::printf("%-8s", alertStr(snapshot.temperatureAlert));
    resetColor();
    std::printf("]           │  │  [");
    setColor(alertColor(snapshot.batteryAlert));
    setColor(1);
    std::printf("%-8s", alertStr(snapshot.batteryAlert));
    resetColor();
    std::printf("]           │  │  [");
    setColor(alertColor(snapshot.motorAlert));
    setColor(1);
    std::printf("%-8s", alertStr(snapshot.motorAlert));
    resetColor();
    std::printf("]          │");

    moveCursor(9, 1);
    std::printf("└──────────────────────┘  └──────────────────────┘  └────────────────────┘");
}

void DashboardUI::renderRTBBanner(RTBState state)
{
    moveCursor(11, 1);
    int color = rtbColor(state);
    setColor(1);
    setColor(color);

    std::printf("┌──────────────────────────────────────────────────────────────────────────┐\n");
    std::printf("│  RTB STATE: %-20s                                        │\n", rtbStr(state));
    std::printf("└──────────────────────────────────────────────────────────────────────────┘");
    resetColor();
}

void DashboardUI::renderMAVLinkPanel()
{
    moveCursor(15, 1);
    setColor(1);
    setColor(33); // yellow
    std::printf("── MAVLink Telemetry ──────────────────────────────────────────────────────");
    resetColor();

    std::lock_guard<std::mutex> lock(logMutex_);
    int row = 16;
    for (auto& line : mavlinkLog_) {
        moveCursor(row++, 1);
        std::printf("  %-74.74s", line.c_str());
    }
    // Clear remaining lines
    for (; row < 16 + static_cast<int>(MAX_LOG_LINES); ++row) {
        moveCursor(row, 1);
        std::printf("  %-74s", "");
    }
}

void DashboardUI::renderEventPanel()
{
    moveCursor(24, 1);
    setColor(1);
    setColor(35); // magenta
    std::printf("── Flight Event Log ───────────────────────────────────────────────────────");
    resetColor();

    std::lock_guard<std::mutex> lock(logMutex_);
    int row = 25;
    for (auto& line : eventLog_) {
        moveCursor(row++, 1);
        std::printf("  %-74.74s", line.c_str());
    }
    for (; row < 25 + static_cast<int>(MAX_LOG_LINES); ++row) {
        moveCursor(row, 1);
        std::printf("  %-74s", "");
    }
}

void DashboardUI::renderProgressBar(float value, float maxVal, int width, int colorCode)
{
    float ratio = value / maxVal;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int filled = static_cast<int>(ratio * width);

    setColor(colorCode);
    std::printf("[");
    for (int i = 0; i < width; ++i) {
        if (i < filled) std::printf("█");
        else std::printf("░");
    }
    std::printf("]");
    resetColor();
}

const char* DashboardUI::alertStr(AlertLevel level)
{
    switch (level) {
    case AlertLevel::NOMINAL:  return "NOMINAL";
    case AlertLevel::WARNING:  return "WARNING";
    case AlertLevel::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

const char* DashboardUI::rtbStr(RTBState state)
{
    switch (state) {
    case RTBState::NOMINAL:         return "NOMINAL";
    case RTBState::DEGRADED:        return "DEGRADED";
    case RTBState::RTB_INITIATED:   return "RTB INITIATED";
    case RTBState::RTB_IN_PROGRESS: return "RTB IN PROGRESS";
    case RTBState::LANDING:         return "LANDING";
    case RTBState::LANDED:          return "LANDED";
    }
    return "UNKNOWN";
}

int DashboardUI::alertColor(AlertLevel level)
{
    switch (level) {
    case AlertLevel::NOMINAL:  return 32; // green
    case AlertLevel::WARNING:  return 33; // yellow
    case AlertLevel::CRITICAL: return 31; // red
    }
    return 37; // white
}

int DashboardUI::rtbColor(RTBState state)
{
    switch (state) {
    case RTBState::NOMINAL:         return 32; // green
    case RTBState::DEGRADED:        return 33; // yellow
    case RTBState::RTB_INITIATED:   return 31; // red
    case RTBState::RTB_IN_PROGRESS: return 31; // red
    case RTBState::LANDING:         return 35; // magenta
    case RTBState::LANDED:          return 36; // cyan
    }
    return 37;
}

} // namespace uav
