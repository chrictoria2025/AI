#pragma once

#include "IFlashStorage.hpp"
#include "SensorTypes.hpp"

#include <array>
#include <cstdint>
#include <mutex>

namespace uav {

static constexpr size_t LOG_BUFFER_SIZE = 256;
static constexpr size_t LOG_ENTRY_SIZE = 96;
static constexpr size_t BATCH_THRESHOLD = 10;

struct alignas(4) LogEntryFixed {
    EventType eventType;
    SubsystemType subsystem;
    uint8_t _pad[2];
    float sensorValue;
    uint64_t timestampUs;
    uint32_t sequenceNumber;
    uint32_t crc32;
    char message[64];
    uint8_t _reserved[8];
};

static_assert(sizeof(LogEntryFixed) == LOG_ENTRY_SIZE, "LogEntryFixed must be exactly 96 bytes");

class EventLogger {
public:
    explicit EventLogger(IFlashStorage& flash);
    ~EventLogger() = default;

    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    bool logEvent(EventType type, SubsystemType subsystem, float sensorValue,
                  uint64_t timestampUs, const char* message);

    uint32_t getEntryCount() const;
    bool readEntry(uint32_t index, LogEntryFixed& out) const;
    bool verifyIntegrity(uint32_t index) const;
    bool verifyAllEntries(uint32_t& firstFailure) const;
    void flush();

private:
    uint32_t computeCRC32(const LogEntryFixed& entry) const;
    bool shouldImmediateFlush(EventType type) const;

    IFlashStorage& flash_;

    mutable std::mutex bufferMutex_;
    std::array<LogEntryFixed, LOG_BUFFER_SIZE> buffer_{};
    uint32_t writeIndex_{0};
    uint32_t entryCount_{0};
    uint32_t nextSequence_{0};

    uint32_t unflushedCount_{0};
    uint32_t flashWriteOffset_{0};
};

} // namespace uav
