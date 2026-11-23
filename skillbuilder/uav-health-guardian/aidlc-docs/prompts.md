# Prompts Log

## Prompt 1 — Project Setup

> We are building an embedded firmware project called UAV Flight Computer Health Guardian. This is a workshop exercise to demonstrate FreeRTOS embedded development patterns using C++17.
>
> Create the following project structure:
>
> uav-health-guardian/
> ├── aidlc-docs/
> │   ├── plans/
> │   ├── requirements/
> │   ├── story-artifacts/
> │   ├── design-artifacts/
> │   └── prompts.md
> ├── include/
> ├── src/
> │   └── hal/
> │       └── posix/
> ├── tests/
> └── cmake/
>
> Throughout this workshop:
> - All planning documents go in aidlc-docs/plans/ as .md files with checkboxes
> - All requirements go in aidlc-docs/requirements/
> - User stories and units go in aidlc-docs/story-artifacts/
> - Architecture documents go in aidlc-docs/design-artifacts/
> - You must plan your work and get my approval before executing
> - Log every prompt I give you to aidlc-docs/prompts.md
>
> Create the folder structure now. Confirm your understanding.

## Prompt 2 — Intent Document

> Your Role: You are a systems engineer specializing in embedded firmware for autonomous vehicles.
>
> Intent:
> A software company is developing firmware for an unmanned aerial vehicle's (UAV) onboard flight computer. The system must continuously monitor flight-critical subsystems — processor temperature, battery cell voltage, and motor controller health — and make autonomous decisions to protect the vehicle and mission.
>
> When parameters breach defined thresholds, the system initiates a controlled Return-To-Base (RTB) sequence, broadcasts status over MAVLink protocol to the ground control station, and logs all events to tamper-proof flash storage for post-mission analysis.
>
> The system must operate under FreeRTOS and support deployment on ARM Cortex-M class processors. It must handle single subsystem failures gracefully without triggering a full RTB sequence.
>
> Technical constraints:
> - Language: C++17
> - RTOS: FreeRTOS (POSIX simulation for development)
> - Protocol: MAVLink v2
> - Logging: Tamper-proof flash with CRC32 per entry
> - Thresholds: Configurable at runtime
> - Timestamps: Microsecond resolution
>
> This is a workshop exercise focused on demonstrating FreeRTOS embedded patterns (tasks, mutexes, queues, HAL abstraction). We are building a health monitoring module, NOT a complete UAV flight system.
>
> Produce a formal Intent Document with these sections:
> 1. System Purpose — one clear statement
> 2. System Context — who/what interacts with this system
> 3. Key Behaviors — 5-7 most important things
> 4. Constraints — technical and operational
> 5. Assumptions
> 6. Success Criteria — measurable
> 7. Out of Scope
>
> Write the intent document to aidlc-docs/requirements/intent.md
> Do not propose solutions or write code yet.

## Prompt 3 — User Stories

> Your Role: You are an expert product manager with embedded systems domain expertise. Before you start, write your plan in aidlc-docs/plans/user_stories_plan.md with checkboxes. Ask for my approval before executing.
>
> Your Task: Using the approved intent document, generate 10-12 user stories for these 5 stakeholders:
> - Ground Control Station (GCS) operator
> - UAV autopilot system
> - Maintenance engineer (post-mission)
> - Program manager / safety officer
> - The UAV vehicle itself (as an autonomous agent)
>
> For each user story:
> - Format: "As a [stakeholder], I need [capability] so that [outcome]"
> - Include 3-5 acceptance criteria
> - Assign priority: CRITICAL / HIGH / MEDIUM
> - Note dependencies between stories
>
> Stories must cover:
> - Normal operation monitoring (all 3 subsystems)
> - Warning threshold behavior
> - Critical threshold and RTB trigger
> - Degraded mode (single subsystem failure without RTB)
> - MAVLink telemetry broadcasting
> - Flash event logging with CRC32 tamper detection
> - Post-mission log retrieval and verification
> - System startup and watchdog recovery
>
> Write user stories to aidlc-docs/story-artifacts/user_stories.md

## Prompt 4 — Unit Decomposition

> Your Role: You are an experienced embedded software architect. Before you start, write your plan in aidlc-docs/plans/units_plan.md with checkboxes. Ask for my approval before executing.
>
> Your Task: Decompose the system into exactly 5 independent, testable units. Use these specific unit names and responsibilities:
>
> Unit 1: Sensor HAL
> - Hardware abstraction layer with pure abstract C++ interface (ISensorHAL)
> - POSIX simulation implementation for development/testing
> - Methods: readCPUTemperature(), readBatteryVoltage(), readMotorControllerHealth(), getTimestampMicros(), isHealthy()
>
> Unit 2: Health Monitor
> - Reads from ISensorHAL, evaluates against configurable thresholds
> - Produces HealthSnapshot with per-subsystem alert levels (NOMINAL/WARNING/CRITICAL)
> - Runs as FreeRTOS task at 100ms period with mutex-protected snapshot access
>
> Unit 3: RTB Manager
> - Receives alerts from HealthMonitor, manages RTB state machine
> - 6 states: NOMINAL, DEGRADED, RTB_INITIATED, RTB_IN_PROGRESS, LANDING, LANDED
> - Specific trigger rules for when to initiate RTB vs enter degraded mode
>
> Unit 4: MAVLink Reporter
> - Formats and broadcasts health telemetry using MAVLink v2 message structures
> - Runs as FreeRTOS task, 500ms normal / 250ms degraded mode
> - Uses ITransport interface for output abstraction
>
> Unit 5: Flight Event Logger
> - Tamper-proof event logging with CRC32 checksums
> - 256-entry circular buffer with batch flash writes
> - Append-only with verifyIntegrity() method
>
> For each unit provide:
> - Responsibility statement
> - Inputs and outputs
> - Dependencies on other units
> - Interface contract
> - Complexity estimate (LOW/MEDIUM/HIGH)
>
> Also provide a dependency graph and recommended build sequence.
>
> Write to aidlc-docs/story-artifacts/units.md

## Prompt 5 — Domain Model

> Your Role: You are an experienced embedded software architect. Before you start, write your plan in aidlc-docs/plans/domain_model_plan.md with checkboxes. Ask for my approval before executing.
>
> Your Task: We are entering the Construction phase. Generate a domain model for the UAV Flight Computer Health Guardian.
>
> The domain model must include:
>
> 1. Component Diagram — Mermaid diagram showing all 5 units and their relationships
>
> 2. Data Flow — how sensor readings flow from HAL -> HealthMonitor -> RTBManager/MAVLinkReporter/EventLogger
>
> 3. Key Data Structures — define these as C++17 structs/enums:
>    - SensorReading (temperature float, voltage float, motorHealth float, timestamp uint64_t)
>    - AlertLevel enum class (NOMINAL, WARNING, CRITICAL)
>    - SubsystemType enum class (CPU_TEMP, BATTERY_VOLTAGE, MOTOR_HEALTH, SYSTEM)
>    - HealthSnapshot (readings + per-subsystem AlertLevel + overall AlertLevel + timestamp)
>    - RTBState enum class (NOMINAL, DEGRADED, RTB_INITIATED, RTB_IN_PROGRESS, LANDING, LANDED)
>    - EventType enum class (STARTUP, NOMINAL, WARNING, CRITICAL, RTB_INITIATED, RTB_COMPLETE, DEGRADED_MODE, WATCHDOG_RESET, SHUTDOWN)
>    - LogEntry (eventType, subsystem, sensorValue, timestamp_us, sequenceNumber, crc32, message[64])
>
> 4. FreeRTOS Task Layout:
>    | Task            | Priority    | Stack      | Period |
>    | HealthMonitor   | HIGH (3)    | 2048 words | 100ms  |
>    | MAVLinkReporter | MEDIUM (2)  | 1024 words | 500ms  |
>    | Watchdog        | HIGHEST (4) | 512 words  | 1000ms |
>
> 5. Key Design Decisions — 3-5 architectural decisions with reasoning
>
> Write to aidlc-docs/design-artifacts/domain_model.md

## Prompt 6 — Unit Implementation Tracker

> Create a file aidlc-docs/plans/unit_implementation_tracker.md with the unit implementation tracker showing all 5 units in dependency order with checkboxes.

## Prompt 7 — Implement Unit 1: Sensor HAL

> Implement Unit 1: Sensor HAL.
>
> Create a pure abstract C++17 interface and a POSIX simulation implementation.
>
> ISensorHAL interface (include/ISensorHAL.hpp):
> - virtual float readCPUTemperature() = 0       — degrees Celsius
> - virtual float readBatteryVoltage() = 0       — volts (nominal 11.1V, 3S LiPo)
> - virtual float readMotorControllerHealth() = 0 — percentage 0-100
> - virtual uint32_t getTimestampMicros() = 0    — microsecond monotonic timestamp
> - virtual bool isHealthy() = 0                 — overall HAL health
>
> Also create include/SensorTypes.hpp with shared enums and structs from the domain model.
>
> PosixSensorHAL (src/hal/posix/PosixSensorHAL.hpp and .cpp):
> - CPU temperature starts at 40C, slowly rises with small random variation
> - Battery voltage starts at 11.8V, gradually depletes
> - Motor health starts at 100%, degrades slowly
> - After ~15 seconds, inject a critical temperature spike (>95C) for demo
> - Use std::chrono for timestamps, no platform-specific headers
>
> Requirements: C++17, no hardware dependencies, thread-safe timestamps, compiles on macOS/Linux/Windows.
>
> After implementation, mark Unit 1 complete in the tracker.

## Prompt 8 — Implement Unit 2: Health Monitor

> Implement Unit 2: Health Monitor.
>
> Create the HealthMonitor class that reads from ISensorHAL and evaluates thresholds.
>
> Thresholds (configurable at runtime):
> - CPU Temperature:   WARNING >= 75C,   CRITICAL > 95C   (higher is worse)
> - Battery Voltage:   WARNING <= 10.5V, CRITICAL <= 9.5V  (lower is worse)
> - Motor Controller:  WARNING <= 70%,   CRITICAL <= 50%   (lower is worse)
>
> Public API:
> - HealthSnapshot getLatestSnapshot() const — thread-safe via mutex
> - void registerAlertCallback(std::function<void(AlertLevel prev, AlertLevel next, const HealthSnapshot&)>)
> - void evaluateOnce() — single evaluation cycle for unit testing without threads
> - void start() / void stop() — start/stop monitoring thread
>
> Behavior:
> - Evaluate each subsystem independently every cycle
> - Overall alert level = highest of all subsystem levels
> - Alert callback fires ONLY when overall level changes
>
> After implementation, mark Unit 2 complete in the tracker.

## Prompt 9 — Implement Unit 3: RTB Manager

> Implement Unit 3: RTB Manager.
>
> RTB Trigger Rules:
> - CRITICAL on 2+ subsystems simultaneously → trigger RTB
> - CRITICAL CPU temperature alone → trigger RTB (flight safety override)
> - CRITICAL battery voltage alone → trigger RTB (power safety override)
> - CRITICAL motor controller alone → enter DEGRADED mode only (NOT RTB)
> - WARNING on any subsystem → enter DEGRADED mode, do NOT trigger RTB
>
> RTB State Machine (6 states):
> NOMINAL → DEGRADED (on WARNING)
> NOMINAL → RTB_INITIATED (on critical RTB trigger)
> DEGRADED → NOMINAL (on all-clear)
> DEGRADED → RTB_INITIATED (on critical RTB trigger)
> RTB_INITIATED → RTB_IN_PROGRESS → LANDING → LANDED
>
> In POSIX build, simulate RTB waypoint sequence with timing delays:
> RTB_INITIATED → (2s) → RTB_IN_PROGRESS → (3s) → LANDING → (2s) → LANDED
>
> After implementation, mark Unit 3 complete in the tracker.

## Prompt 10 — Implement Unit 4: MAVLink Reporter

> Implement Unit 4: MAVLink Reporter.
>
> MAVLink Messages to implement (struct-based, no external MAVLink library):
> 1. HEARTBEAT (id 0) — every 1000ms
> 2. SYS_STATUS (id 1) — every 500ms
> 3. STATUSTEXT (id 253) — on alert level change
> 4. Custom health payload — every 500ms
>
> Transport Abstraction:
> - ITransport interface: virtual void send(const uint8_t* data, size_t len) = 0
> - StdoutTransport for POSIX: prints human-readable formatted output
>
> Expected output format:
> [   500ms] [MAVLINK HTH] TEMP=42.3C  VOLT=11.8V  MOTOR=98%  ALERT=NOMINAL  RTB=NOMINAL
>
> After implementation, mark Unit 4 complete in the tracker.

## Prompt 11 — Implement Unit 5: Flight Event Logger

> Implement Unit 5: Flight Event Logger.
>
> Log Entry structure (fixed 96 bytes per record):
> - EventType type
> - SubsystemType subsystem
> - float sensorValue
> - uint64_t timestampUs (microseconds)
> - uint32_t sequenceNumber (monotonically increasing from 0)
> - uint32_t crc32 (computed over ALL fields except crc32 itself)
> - char message[64] (human-readable)
>
> Tamper-Proof Requirements:
> - CRC32 checksum over all fields except the checksum
> - Sequence numbers contiguous — gaps indicate deleted entries
> - Append-only — NO deletion or modification methods on public API
>
> Storage:
> - In-memory circular buffer: 256 entries
> - Batch write to flash every 10 entries OR immediately on CRITICAL/RTB events
> - POSIX: simulate flash as binary file "flight_log.bin"
>
> After implementation, mark Unit 5 complete in the tracker.

## Prompt 12 — Catch2 Unit Tests

> Generate Catch2 unit tests for the UAV Health Guardian — one test file per unit, 5 test files total.
>
> Requirements:
> - Catch2 v3 (fetched via FetchContent, not manual install)
> - NO FreeRTOS dependency in tests — use std::thread or direct method calls
> - Tests compile and run on macOS/Linux/Windows without hardware
>
> Build and run:
> cmake -B build -DBUILD_TARGET=posix && cmake --build build && cd build && ctest --output-on-failure
>
> Expected result: 100% tests passed, 0 tests failed out of 5.

## Prompt 13 — Multi-Target CMake + TUI Dashboard + main.cpp

> Generate a multi-target CMake build configuration and a live TUI dashboard.
>
> Target 1: posix (default) - macOS / Linux / Windows development
> Target 2: pico (placeholder) - Raspberry Pi Pico W
> Target 3: esp32s3 (placeholder) - ESP32-S3 via ESP-IDF
>
> Generate a DashboardUI class with HUD-style terminal display using ANSI escape codes showing:
> - Three sensor columns with progress bars and alert badges
> - RTB state banner with color coding
> - MAVLink telemetry log panel
> - Flight event log panel
>
> Also generate main.cpp that wires all 5 units together and runs the simulation.
>
> Build, test, and run:
> cmake -B build -DBUILD_TARGET=posix && cmake --build build && cd build && ctest --output-on-failure && ./uav_health_guardian_posix
