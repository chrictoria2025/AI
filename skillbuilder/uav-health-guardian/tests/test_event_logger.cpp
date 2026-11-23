#include <catch2/catch_test_macros.hpp>

#include "EventLogger.hpp"
#include "IFlashStorage.hpp"

#include <array>
#include <cstring>
#include <vector>

using namespace uav;

class MockFlashStorage : public IFlashStorage {
public:
    std::vector<uint8_t> storage;
    size_t cap;
    size_t writeCount = 0;

    explicit MockFlashStorage(size_t capacity = 256 * LOG_ENTRY_SIZE)
        : cap(capacity) {
        storage.resize(cap, 0);
    }

    bool write(const uint8_t* data, size_t offset, size_t length) override {
        if (offset + length > cap) return false;
        std::memcpy(storage.data() + offset, data, length);
        ++writeCount;
        return true;
    }

    bool read(uint8_t* buffer, size_t offset, size_t length) override {
        if (offset + length > cap) return false;
        std::memcpy(buffer, storage.data() + offset, length);
        return true;
    }

    size_t capacity() const override { return cap; }
};

TEST_CASE("EventLogger starts with zero entries", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);
    REQUIRE(logger.getEntryCount() == 0);
}

TEST_CASE("EventLogger logs an event and increments count", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    bool ok = logger.logEvent(EventType::STARTUP, SubsystemType::SYSTEM, 0.0f, 1000, "System boot");
    REQUIRE(ok);
    REQUIRE(logger.getEntryCount() == 1);
}

TEST_CASE("EventLogger entries have monotonic sequence numbers", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    logger.logEvent(EventType::NOMINAL, SubsystemType::CPU_TEMP, 40.0f, 1000, "Normal");
    logger.logEvent(EventType::WARNING, SubsystemType::CPU_TEMP, 80.0f, 2000, "Warming");
    logger.logEvent(EventType::CRITICAL, SubsystemType::CPU_TEMP, 96.0f, 3000, "Overheating");

    LogEntryFixed e0{}, e1{}, e2{};
    REQUIRE(logger.readEntry(0, e0));
    REQUIRE(logger.readEntry(1, e1));
    REQUIRE(logger.readEntry(2, e2));

    REQUIRE(e0.sequenceNumber == 0);
    REQUIRE(e1.sequenceNumber == 1);
    REQUIRE(e2.sequenceNumber == 2);
}

TEST_CASE("EventLogger CRC32 integrity verification passes", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    logger.logEvent(EventType::WARNING, SubsystemType::BATTERY_VOLTAGE, 10.2f, 5000, "Low battery");

    REQUIRE(logger.verifyIntegrity(0));
}

TEST_CASE("EventLogger verifyAllEntries passes on clean log", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    for (int i = 0; i < 5; ++i) {
        logger.logEvent(EventType::NOMINAL, SubsystemType::MOTOR_HEALTH, 95.0f,
                        static_cast<uint64_t>(i * 100), "OK");
    }

    uint32_t firstFailure = 0;
    REQUIRE(logger.verifyAllEntries(firstFailure));
}

TEST_CASE("EventLogger stores correct field values", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    logger.logEvent(EventType::RTB_INITIATED, SubsystemType::CPU_TEMP, 97.5f, 12345678, "RTB due to temp");

    LogEntryFixed entry{};
    REQUIRE(logger.readEntry(0, entry));

    REQUIRE(entry.eventType == EventType::RTB_INITIATED);
    REQUIRE(entry.subsystem == SubsystemType::CPU_TEMP);
    REQUIRE(entry.sensorValue == 97.5f);
    REQUIRE(entry.timestampUs == 12345678);
    REQUIRE(std::string(entry.message) == "RTB due to temp");
}

TEST_CASE("EventLogger batch flushes to flash every 10 entries", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    // Log 9 nominal entries — should not flush yet
    for (int i = 0; i < 9; ++i) {
        logger.logEvent(EventType::NOMINAL, SubsystemType::SYSTEM, 0.0f,
                        static_cast<uint64_t>(i * 100), "tick");
    }
    REQUIRE(flash.writeCount == 0);

    // 10th entry triggers batch flush
    logger.logEvent(EventType::NOMINAL, SubsystemType::SYSTEM, 0.0f, 900, "tick");
    REQUIRE(flash.writeCount == 10);
}

TEST_CASE("EventLogger immediate flush on CRITICAL event", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    logger.logEvent(EventType::CRITICAL, SubsystemType::CPU_TEMP, 98.0f, 1000, "Overheat");
    REQUIRE(flash.writeCount == 1);
}

TEST_CASE("EventLogger immediate flush on RTB_INITIATED event", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    logger.logEvent(EventType::RTB_INITIATED, SubsystemType::SYSTEM, 0.0f, 2000, "RTB triggered");
    REQUIRE(flash.writeCount == 1);
}

TEST_CASE("EventLogger circular buffer wraps at 256 entries", "[event_logger]") {
    MockFlashStorage flash(512 * LOG_ENTRY_SIZE);
    EventLogger logger(flash);

    for (uint32_t i = 0; i < 300; ++i) {
        logger.logEvent(EventType::NOMINAL, SubsystemType::SYSTEM, static_cast<float>(i),
                        static_cast<uint64_t>(i * 100), "wrap");
    }

    // Buffer holds max 256 entries
    REQUIRE(logger.getEntryCount() == 256);

    // First available entry should have sequence 44 (300 - 256)
    LogEntryFixed entry{};
    REQUIRE(logger.readEntry(0, entry));
    REQUIRE(entry.sequenceNumber == 44);
}

TEST_CASE("EventLogger message truncation at 63 chars", "[event_logger]") {
    MockFlashStorage flash;
    EventLogger logger(flash);

    std::string longMsg(100, 'A');
    logger.logEvent(EventType::NOMINAL, SubsystemType::SYSTEM, 0.0f, 1000, longMsg.c_str());

    LogEntryFixed entry{};
    REQUIRE(logger.readEntry(0, entry));
    REQUIRE(std::strlen(entry.message) == 63);
}
