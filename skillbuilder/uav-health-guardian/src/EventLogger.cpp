#include "EventLogger.hpp"

#include <cstring>

namespace uav {

EventLogger::EventLogger(IFlashStorage& flash)
    : flash_(flash)
{
}

bool EventLogger::logEvent(EventType type, SubsystemType subsystem, float sensorValue,
                           uint64_t timestampUs, const char* message)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    LogEntryFixed entry{};
    entry.eventType = type;
    entry.subsystem = subsystem;
    entry.sensorValue = sensorValue;
    entry.timestampUs = timestampUs;
    entry.sequenceNumber = nextSequence_++;
    entry.crc32 = 0;

    if (message) {
        std::strncpy(entry.message, message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';
    }

    entry.crc32 = computeCRC32(entry);

    buffer_[writeIndex_] = entry;
    writeIndex_ = (writeIndex_ + 1) % LOG_BUFFER_SIZE;

    if (entryCount_ < LOG_BUFFER_SIZE) {
        ++entryCount_;
    }

    ++unflushedCount_;

    if (shouldImmediateFlush(type) || unflushedCount_ >= BATCH_THRESHOLD) {
        flush();
    }

    return true;
}

uint32_t EventLogger::getEntryCount() const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return entryCount_;
}

bool EventLogger::readEntry(uint32_t index, LogEntryFixed& out) const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (index >= entryCount_) {
        return false;
    }

    uint32_t bufferIndex;
    if (entryCount_ < LOG_BUFFER_SIZE) {
        bufferIndex = index;
    } else {
        bufferIndex = (writeIndex_ + index) % LOG_BUFFER_SIZE;
    }

    out = buffer_[bufferIndex];
    return true;
}

bool EventLogger::verifyIntegrity(uint32_t index) const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (index >= entryCount_) {
        return false;
    }

    uint32_t bufferIndex;
    if (entryCount_ < LOG_BUFFER_SIZE) {
        bufferIndex = index;
    } else {
        bufferIndex = (writeIndex_ + index) % LOG_BUFFER_SIZE;
    }

    const LogEntryFixed& entry = buffer_[bufferIndex];
    uint32_t expected = computeCRC32(entry);
    return entry.crc32 == expected;
}

bool EventLogger::verifyAllEntries(uint32_t& firstFailure) const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);

    for (uint32_t i = 0; i < entryCount_; ++i) {
        uint32_t bufferIndex;
        if (entryCount_ < LOG_BUFFER_SIZE) {
            bufferIndex = i;
        } else {
            bufferIndex = (writeIndex_ + i) % LOG_BUFFER_SIZE;
        }

        const LogEntryFixed& entry = buffer_[bufferIndex];
        uint32_t expected = computeCRC32(entry);
        if (entry.crc32 != expected) {
            firstFailure = i;
            return false;
        }
    }

    return true;
}

void EventLogger::flush()
{
    if (unflushedCount_ == 0) {
        return;
    }

    uint32_t startIndex;
    if (writeIndex_ >= unflushedCount_) {
        startIndex = writeIndex_ - unflushedCount_;
    } else {
        startIndex = LOG_BUFFER_SIZE - (unflushedCount_ - writeIndex_);
    }

    for (uint32_t i = 0; i < unflushedCount_; ++i) {
        uint32_t idx = (startIndex + i) % LOG_BUFFER_SIZE;
        const auto* data = reinterpret_cast<const uint8_t*>(&buffer_[idx]);
        flash_.write(data, flashWriteOffset_, LOG_ENTRY_SIZE);
        flashWriteOffset_ += LOG_ENTRY_SIZE;
    }

    unflushedCount_ = 0;
}

uint32_t EventLogger::computeCRC32(const LogEntryFixed& entry) const
{
    // CRC32 over all fields except the crc32 field itself
    // crc32 is at offset 16 (after eventType[1] + subsystem[1] + pad[2] + sensorValue[4] + timestampUs[8])
    // and is 4 bytes wide. Total struct is 96 bytes.
    static constexpr size_t CRC_FIELD_OFFSET = offsetof(LogEntryFixed, crc32);
    static constexpr size_t CRC_FIELD_SIZE = sizeof(uint32_t);

    const auto* bytes = reinterpret_cast<const uint8_t*>(&entry);

    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < LOG_ENTRY_SIZE; ++i) {
        if (i >= CRC_FIELD_OFFSET && i < CRC_FIELD_OFFSET + CRC_FIELD_SIZE) {
            continue;
        }

        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

bool EventLogger::shouldImmediateFlush(EventType type) const
{
    return type == EventType::CRITICAL ||
           type == EventType::RTB_INITIATED ||
           type == EventType::SHUTDOWN;
}

} // namespace uav
