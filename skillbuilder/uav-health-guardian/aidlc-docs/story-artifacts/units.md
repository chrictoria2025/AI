# System Unit Decomposition — UAV Flight Computer Health Guardian

## Overview

| Unit | Name | Responsibility | Complexity |
|------|------|---------------|------------|
| 1 | Sensor HAL | Abstract hardware access; provide POSIX simulation | LOW |
| 2 | Health Monitor | Sample sensors, evaluate thresholds, produce health snapshots | MEDIUM |
| 3 | RTB Manager | Manage RTB state machine based on health alerts | HIGH |
| 4 | MAVLink Reporter | Format and broadcast telemetry to GCS | MEDIUM |
| 5 | Flight Event Logger | Tamper-proof event logging with CRC32 integrity | MEDIUM |

---

## Unit 1: Sensor HAL

### Responsibility
Provide a hardware-independent interface for reading flight-critical sensor data. Isolate all platform-specific code behind a pure abstract interface so that the rest of the system can be developed and tested on a POSIX host.

### Interface Contract

```cpp
struct SensorReading {
    float value;
    uint64_t timestamp_us;
    bool valid;
};

class ISensorHAL {
public:
    virtual ~ISensorHAL() = default;
    virtual SensorReading readCPUTemperature() = 0;
    virtual SensorReading readBatteryVoltage() = 0;
    virtual SensorReading readMotorControllerHealth() = 0;
    virtual uint64_t getTimestampMicros() = 0;
    virtual bool isHealthy() = 0;
};
```

### Inputs
- Physical sensor registers (on target hardware)
- System clock (for microsecond timestamps)
- Simulated values / fault injection (in POSIX implementation)

### Outputs
- `SensorReading` structs with value, timestamp, and validity flag
- Boolean health status indicating HAL layer connectivity

### Dependencies
- None (leaf unit — depends only on platform primitives)

### POSIX Simulation Implementation
- `PosixSensorHAL` class implements `ISensorHAL`
- Returns configurable baseline values with optional noise
- Supports fault injection: `injectFault(subsystem, faultType)` for testing
- Uses `gettimeofday()` or `clock_gettime(CLOCK_MONOTONIC)` for timestamps

### Complexity: LOW
- Simple interface, no state management, no concurrency concerns
- POSIX implementation is straightforward value generation

---

## Unit 2: Health Monitor

### Responsibility
Periodically sample all sensors via `ISensorHAL`, evaluate each reading against configurable warning and critical thresholds, and produce a mutex-protected `HealthSnapshot` that other units can read concurrently.

### Interface Contract

```cpp
enum class AlertLevel { NOMINAL, WARNING, CRITICAL };

struct SubsystemStatus {
    AlertLevel level;
    float last_reading;
    uint64_t timestamp_us;
    uint32_t consecutive_warnings;
};

struct HealthSnapshot {
    SubsystemStatus temperature;
    SubsystemStatus battery;
    SubsystemStatus motor;
    uint64_t snapshot_timestamp_us;
    uint32_t sequence_number;
};

struct ThresholdConfig {
    float warning_high;
    float critical_high;
    float warning_low;
    float critical_low;
};

class IHealthMonitor {
public:
    virtual ~IHealthMonitor() = default;
    virtual HealthSnapshot getSnapshot() const = 0;
    virtual void updateThresholds(/* subsystem */, const ThresholdConfig&) = 0;
    virtual bool isRunning() const = 0;
};
```

### Inputs
- `ISensorHAL` — sensor readings at each sample cycle
- `ThresholdConfig` — runtime-configurable thresholds per subsystem
- FreeRTOS tick — drives the 100 ms task period

### Outputs
- `HealthSnapshot` — mutex-protected, updated every 100 ms
- State transition events (emitted to subscribers: RTB Manager, Logger, Reporter)

### Dependencies
- **Unit 1 (Sensor HAL)** — injected via `ISensorHAL` pointer at construction

### FreeRTOS Integration
- Runs as a FreeRTOS task at `configMAX_PRIORITIES - 2` (high priority)
- 100 ms period via `vTaskDelayUntil()`
- Snapshot access protected by a FreeRTOS mutex (`xSemaphoreTakeRecursive`)
- Threshold updates also mutex-protected (same mutex)

### State Transition Logic
- NOMINAL → WARNING: reading crosses warning threshold
- WARNING → CRITICAL: reading crosses critical threshold
- WARNING → NOMINAL: reading returns below warning threshold (with hysteresis)
- Any state → NOMINAL: configurable recovery debounce (N consecutive good readings)

### Complexity: MEDIUM
- Threshold evaluation logic is straightforward
- Concurrency management (mutex, task timing) adds moderate complexity
- Hysteresis and debounce require careful state tracking

---

## Unit 3: RTB Manager

### Responsibility
Consume health snapshots and state transition events, manage the vehicle protection state machine, and issue RTB commands to the autopilot when escalation rules are met. Implement single-fault tolerance (degraded mode) without unnecessary RTB triggers.

### Interface Contract

```cpp
enum class RTBState {
    NOMINAL,
    DEGRADED,
    RTB_INITIATED,
    RTB_IN_PROGRESS,
    LANDING,
    LANDED
};

struct RTBCommand {
    RTBState requested_state;
    uint8_t trigger_subsystem_mask;
    AlertLevel trigger_level;
    uint64_t timestamp_us;
};

class IRTBManager {
public:
    virtual ~IRTBManager() = default;
    virtual RTBState getState() const = 0;
    virtual void onHealthUpdate(const HealthSnapshot&) = 0;
    virtual void onAutopilotAck(bool success) = 0;
};
```

### Inputs
- `HealthSnapshot` — from Health Monitor (polled or event-driven)
- Autopilot acknowledgement — confirms RTB command was received
- Timer events — for timeout-based escalation

### Outputs
- `RTBCommand` — placed on FreeRTOS queue for autopilot consumption
- State transition events — consumed by Logger and Reporter

### Dependencies
- **Unit 2 (Health Monitor)** — receives health snapshots

### State Machine Transitions

```
NOMINAL ──[single subsystem WARNING]──→ NOMINAL (no change)
NOMINAL ──[single subsystem CRITICAL]──→ RTB_INITIATED
NOMINAL ──[≥2 subsystems WARNING]──→ RTB_INITIATED
NOMINAL ──[single subsystem fault, non-critical]──→ DEGRADED
DEGRADED ──[fault cleared]──→ NOMINAL
DEGRADED ──[second subsystem fault]──→ RTB_INITIATED
DEGRADED ──[any CRITICAL]──→ RTB_INITIATED
RTB_INITIATED ──[autopilot ACK]──→ RTB_IN_PROGRESS
RTB_INITIATED ──[ACK timeout 200ms]──→ RTB_INITIATED (retry once)
RTB_IN_PROGRESS ──[altitude < threshold]──→ LANDING
LANDING ──[touchdown detected]──→ LANDED
```

### Trigger Rules (Summarized)
1. **Single CRITICAL** → immediate RTB
2. **Two or more simultaneous WARNINGs** → RTB
3. **Single non-critical fault** → DEGRADED (continue mission)
4. **Second fault while DEGRADED** → RTB
5. **Watchdog triple-fault** (from Unit 2 events) → RTB

### Complexity: HIGH
- 6-state machine with multiple transition triggers
- Timeout handling and retry logic
- Must be formally verifiable (all transitions explicit, no implicit states)
- Concurrency: receives events from monitor task, sends to autopilot queue

---

## Unit 4: MAVLink Reporter

### Responsibility
Format system health data into MAVLink v2 message structures and transmit them at appropriate intervals to the ground control station. Adapt reporting rate based on system state.

### Interface Contract

```cpp
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool send(const uint8_t* data, size_t length) = 0;
    virtual bool isReady() const = 0;
};

class IMAVLinkReporter {
public:
    virtual ~IMAVLinkReporter() = default;
    virtual void onHealthUpdate(const HealthSnapshot&) = 0;
    virtual void onStateChange(RTBState old_state, RTBState new_state) = 0;
    virtual uint32_t getMessagesSent() const = 0;
    virtual uint32_t getMessagesDropped() const = 0;
};
```

### Inputs
- `HealthSnapshot` — current system health (from Health Monitor)
- `RTBState` — current vehicle protection state (from RTB Manager)
- FreeRTOS tick — drives periodic reporting

### Outputs
- Formatted MAVLink v2 messages via `ITransport` interface
- Diagnostic counters (messages sent, dropped)

### Dependencies
- **Unit 2 (Health Monitor)** — reads health snapshots for telemetry content
- **Unit 3 (RTB Manager)** — reads RTB state for state-change messages

### MAVLink Message Types
1. **Heartbeat** (periodic) — system status summary, sent every 500 ms (normal) or 250 ms (degraded/RTB)
2. **Health Alert** (event-driven) — immediate on state transition (WARNING/CRITICAL)
3. **RTB Status** (event-driven) — sent when RTB state machine transitions

### FreeRTOS Integration
- Runs as a FreeRTOS task at `configMAX_PRIORITIES - 3` (below monitor, above logger)
- Adaptive period: 500 ms in NOMINAL, 250 ms in DEGRADED or RTB states
- Outbound message queue (depth: 16 messages)
- Queue-full policy: drop oldest non-critical message; never drop CRITICAL alerts

### Complexity: MEDIUM
- MAVLink v2 framing is well-defined but requires correct sequence numbering and CRC
- Adaptive rate logic is straightforward
- Queue management with priority-based drop policy adds moderate complexity

---

## Unit 5: Flight Event Logger

### Responsibility
Record all health events, state transitions, and system decisions to persistent storage with CRC32 integrity protection per entry. Provide post-mission retrieval and verification capabilities.

### Interface Contract

```cpp
enum class EventType : uint8_t {
    STARTUP,
    SHUTDOWN,
    THRESHOLD_WARNING,
    THRESHOLD_CRITICAL,
    STATE_TRANSITION,
    RTB_INITIATED,
    RTB_COMPLETED,
    WATCHDOG_RESTART,
    CONFIG_CHANGE,
    SENSOR_FAULT
};

struct LogEntry {
    uint64_t timestamp_us;
    EventType event_type;
    uint8_t subsystem_id;
    float reading_value;
    uint8_t metadata[8];
    uint32_t crc32;
};

class IFlashStorage {
public:
    virtual ~IFlashStorage() = default;
    virtual bool write(const uint8_t* data, size_t offset, size_t length) = 0;
    virtual bool read(uint8_t* buffer, size_t offset, size_t length) = 0;
    virtual size_t capacity() const = 0;
};

class IEventLogger {
public:
    virtual ~IEventLogger() = default;
    virtual bool logEvent(EventType type, uint8_t subsystem, float value,
                          const uint8_t* metadata = nullptr) = 0;
    virtual uint32_t getEntryCount() const = 0;
    virtual bool readEntry(uint32_t index, LogEntry& out) const = 0;
    virtual bool verifyIntegrity(uint32_t index) const = 0;
    virtual bool verifyAllEntries(uint32_t& first_failure) const = 0;
};
```

### Inputs
- Events from all other units (state transitions, threshold breaches, RTB decisions, startup/shutdown)
- Microsecond timestamps from HAL

### Outputs
- CRC32-protected log entries in flash storage
- Integrity verification results (per-entry and batch)

### Dependencies
- **Unit 1 (Sensor HAL)** — for `getTimestampMicros()` (timestamp source)
- **Unit 2 (Health Monitor)** — receives threshold breach events
- **Unit 3 (RTB Manager)** — receives RTB state transition events

### Storage Design
- 256-entry circular buffer in RAM (write staging area)
- Batch flush to flash via `IFlashStorage` when buffer reaches high-water mark (75% = 192 entries) or on critical event
- Each entry is 32 bytes (fixed size for direct indexing)
- Append-only semantics — write pointer only advances, never retreats
- Total flash allocation: configurable (default 256 KB = 8192 entries)

### CRC32 Integrity
- CRC32 computed over all fields of `LogEntry` excluding the `crc32` field itself
- Polynomial: ISO 3309 / ITU-T (0xEDB88320, reflected)
- Verification: recompute CRC on readback, compare against stored value

### FreeRTOS Integration
- Runs as a FreeRTOS task at `configMAX_PRIORITIES - 4` (lowest of the Health Guardian tasks)
- Receives events via FreeRTOS queue (depth: 32 events)
- Flash writes are non-blocking to the event producers (queue decouples timing)
- If queue is full, event is counted as dropped (metric available via `getMessagesDropped()`)

### Complexity: MEDIUM
- Circular buffer and batch write logic require careful index management
- CRC32 computation is algorithmic but well-understood
- Flash write abstraction keeps the unit testable without real hardware
- No complex state machine — primarily append-and-verify

---

## Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│                      POSIX / Hardware                         │
└─────────────────────────┬───────────────────────────────────┘
                          │
                ┌─────────▼─────────┐
                │   Unit 1: Sensor  │
                │       HAL         │
                │   (ISensorHAL)    │
                └─────────┬─────────┘
                          │
                ┌─────────▼─────────┐
                │  Unit 2: Health   │
                │     Monitor       │
                │ (IHealthMonitor)  │
                └──┬──────┬──────┬──┘
                   │      │      │
         ┌─────────▼┐  ┌─▼──────┴────────┐
         │  Unit 3:  │  │    Unit 4:       │
         │    RTB    │  │    MAVLink       │
         │  Manager  │  │    Reporter      │
         │(IRTBMgr)  │  │(IMAVLinkReporter)│
         └─────┬─────┘  └────────┬────────┘
               │                  │
               │   ┌──────────────┘
               │   │
         ┌─────▼───▼─────┐
         │   Unit 5:      │
         │  Flight Event  │
         │    Logger      │
         │(IEventLogger)  │
         └────────────────┘
```

### Dependency Matrix

| Unit | Depends On | Depended On By |
|------|-----------|----------------|
| 1 — Sensor HAL | (none) | 2, 5 |
| 2 — Health Monitor | 1 | 3, 4, 5 |
| 3 — RTB Manager | 2 | 4, 5 |
| 4 — MAVLink Reporter | 2, 3 | (none) |
| 5 — Flight Event Logger | 1, 2, 3 | (none) |

---

## Recommended Build Sequence

| Phase | Unit | Rationale |
|-------|------|-----------|
| **Phase 1** | Unit 1: Sensor HAL | Zero dependencies. Establishes the foundation. POSIX simulation enables all subsequent testing. |
| **Phase 2** | Unit 2: Health Monitor | Depends only on Unit 1. Core data producer for the entire system. Once complete, remaining units can be tested against real snapshots. |
| **Phase 3a** | Unit 3: RTB Manager | Depends on Unit 2. Critical safety logic — build early to allow maximum testing time. |
| **Phase 3b** | Unit 5: Flight Event Logger | Depends on Units 1, 2, 3. Can begin interface work in parallel with Unit 3; integration after Unit 3 completes. |
| **Phase 3c** | Unit 4: MAVLink Reporter | Depends on Units 2, 3. Protocol formatting is well-defined; can proceed once Unit 3 interface is stable. |

Phases 3a, 3b, and 3c can proceed in parallel once their dependency interfaces are defined (even before implementations are complete, using mocks).

---

## User Story Traceability

| Unit | Enables User Stories |
|------|---------------------|
| Unit 1: Sensor HAL | US-01, US-02 |
| Unit 2: Health Monitor | US-02, US-03, US-04, US-08, US-12 |
| Unit 3: RTB Manager | US-04, US-05, US-06, US-12 |
| Unit 4: MAVLink Reporter | US-07, US-08 |
| Unit 5: Flight Event Logger | US-09, US-10, US-11 |

All 12 user stories are covered. US-11 (Safety Audit) is satisfied by the test suite across all units.
