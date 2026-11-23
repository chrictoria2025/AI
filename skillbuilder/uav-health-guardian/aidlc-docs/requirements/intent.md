# Intent Document — UAV Flight Computer Health Guardian

## 1. System Purpose

The UAV Flight Computer Health Guardian continuously monitors flight-critical subsystems on an unmanned aerial vehicle's onboard computer, autonomously initiates protective actions when parameters breach configurable thresholds, and provides real-time telemetry and tamper-proof event logging for post-mission analysis.

## 2. System Context

### Internal Actors
- **Health Monitor Module** — the system under development; runs as FreeRTOS tasks on the flight computer
- **Flight Controller** — receives RTB commands from the Health Guardian when critical thresholds are breached
- **Onboard Flash Storage** — receives tamper-proof log entries with CRC32 integrity verification

### External Actors
- **Ground Control Station (GCS)** — receives MAVLink v2 status broadcasts; human operators observe system health remotely
- **Hardware Subsystems** — processor thermal sensor, battery management system (per-cell voltage), motor controller diagnostics

### Interfaces
| Interface | Direction | Protocol/Mechanism |
|-----------|-----------|-------------------|
| Sensor readings | Subsystems → Health Guardian | HAL-abstracted hardware registers |
| Status telemetry | Health Guardian → GCS | MAVLink v2 |
| RTB command | Health Guardian → Flight Controller | Internal command queue |
| Event log entries | Health Guardian → Flash | CRC32-protected writes |
| Threshold config | GCS → Health Guardian | Runtime-configurable parameters |

## 3. Key Behaviors

1. **Continuous Monitoring** — Sample processor temperature, battery cell voltages, and motor controller health at deterministic intervals under FreeRTOS scheduling.

2. **Threshold Evaluation** — Compare each reading against configurable warning and critical thresholds; distinguish between degraded and critical states.

3. **Graceful Degradation** — Handle single subsystem failures (e.g., one motor controller reporting fault) without triggering a full RTB sequence; escalate only when multiple subsystems fail or a single subsystem reaches critical state.

4. **Return-To-Base Initiation** — When critical thresholds are breached across defined escalation rules, autonomously command the flight controller to execute a controlled RTB sequence.

5. **MAVLink Status Broadcast** — Transmit subsystem health status to the ground control station at regular intervals and immediately upon state transitions.

6. **Tamper-Proof Logging** — Write all threshold events, state transitions, and RTB decisions to flash storage with CRC32 integrity per entry and microsecond-resolution timestamps.

7. **Runtime Configuration** — Accept threshold parameter updates without requiring system restart or redeployment.

## 4. Constraints

### Technical
- Language: C++17 (no exceptions, no dynamic allocation after init)
- RTOS: FreeRTOS kernel; all concurrency via FreeRTOS primitives (tasks, queues, mutexes, semaphores)
- Target: ARM Cortex-M class processors (development via POSIX simulation HAL)
- Protocol: MAVLink v2 message framing
- Logging: Each flash entry protected by CRC32; entries must be append-only
- Timing: Microsecond-resolution timestamps on all logged events
- Thresholds: Configurable at runtime without recompilation

### Operational
- Must not interfere with flight controller's real-time control loop
- Must operate within the memory and CPU budget of a Cortex-M4/M7 class device
- Must remain deterministic — no unbounded execution paths

## 5. Assumptions

1. The flight controller exposes a command interface for RTB initiation (we define the interface, not the implementation).
2. Hardware sensors provide readings via memory-mapped registers or similar mechanism abstracted by a HAL layer.
3. FreeRTOS is pre-configured and running; the Health Guardian registers its own tasks at startup.
4. MAVLink transport layer (UART/radio) is available; the Health Guardian is responsible for message construction, not physical transmission.
5. Flash storage driver exists beneath the HAL; the Health Guardian is responsible for entry formatting and CRC, not raw flash management.
6. A single flight computer hosts this module — no distributed consensus is required.
7. Clock source for microsecond timestamps is available via the HAL.

## 6. Success Criteria

| # | Criterion | Measure |
|---|-----------|---------|
| SC-1 | All three subsystems monitored concurrently | Three independent FreeRTOS tasks running, each sampling at its configured rate |
| SC-2 | Threshold breach detected within one sample period | Time from breach condition to state transition ≤ configured sample interval |
| SC-3 | Single-fault tolerance demonstrated | One subsystem in fault state does not trigger RTB; system continues monitoring |
| SC-4 | RTB initiated on critical condition | Multi-subsystem failure or single critical breach triggers RTB command within 100 ms |
| SC-5 | GCS receives status updates | MAVLink heartbeat and event messages constructed and queued correctly |
| SC-6 | Log entries verifiable | Every logged entry passes CRC32 verification on readback |
| SC-7 | Thresholds changeable at runtime | New threshold values take effect on next sample cycle without restart |
| SC-8 | POSIX simulation passes all tests | Full Catch2 test suite passes on host using POSIX HAL |

## 7. Out of Scope

- Flight control algorithms (PID loops, navigation, attitude control)
- Physical sensor drivers or board support packages
- MAVLink transport layer (UART, radio link management)
- Flash memory wear leveling or filesystem management
- Ground control station software
- Over-the-air firmware update mechanism
- Multi-vehicle coordination
- GPS/INS subsystem monitoring
- Power distribution unit control
- Complete UAV flight system — this is a health monitoring module only
