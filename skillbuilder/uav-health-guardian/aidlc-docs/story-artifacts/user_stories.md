# User Stories — UAV Flight Computer Health Guardian

---

## US-01: System Startup and Self-Check

**Priority:** CRITICAL  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need to initialize all health monitoring tasks and verify sensor connectivity at startup so that I can confirm flight-readiness before accepting mission commands.

### Acceptance Criteria
1. All three monitoring tasks (thermal, battery, motor) register with the FreeRTOS scheduler and reach running state within 500 ms of boot.
2. Each sensor HAL channel returns a valid reading during the self-check sequence; a failed channel is reported to the event log.
3. The system publishes a MAVLink startup-complete status message to GCS upon successful initialization.
4. If any critical subsystem fails self-check, the system enters a grounded/no-fly state and logs the failure reason.

### Dependencies
- None (root story)

---

## US-02: Continuous Subsystem Monitoring

**Priority:** CRITICAL  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need to continuously sample processor temperature, battery cell voltages, and motor controller health at deterministic intervals so that I can detect anomalies before they threaten flight safety.

### Acceptance Criteria
1. Processor temperature is sampled at a configurable rate (default: 10 Hz).
2. Battery cell voltages (per-cell) are sampled at a configurable rate (default: 5 Hz).
3. Motor controller health status is sampled at a configurable rate (default: 10 Hz).
4. Each sample carries a microsecond-resolution timestamp from the HAL clock source.
5. Readings are placed on a FreeRTOS queue for threshold evaluation within the same sample period.

### Dependencies
- US-01 (startup must complete before monitoring begins)

---

## US-03: Warning Threshold Detection

**Priority:** CRITICAL  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need to detect when any subsystem reading crosses a warning threshold so that I can alert the operator and begin preparing for potential degradation.

### Acceptance Criteria
1. Warning thresholds are defined per subsystem (temperature high, voltage low, motor fault count) and are configurable at runtime.
2. When a reading crosses from normal to warning state, an event is logged with timestamp and reading value.
3. A MAVLink warning message is transmitted to GCS within one sample period of detection.
4. The system remains in normal operational mode (no RTB) while only warning thresholds are breached.
5. Repeated warnings for the same subsystem do not flood the event log — state transitions only.

### Dependencies
- US-02 (requires continuous monitoring to be active)

---

## US-04: Critical Threshold and RTB Trigger

**Priority:** CRITICAL  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need to initiate a controlled Return-To-Base sequence when critical thresholds are breached so that I can protect the airframe and payload from catastrophic failure.

### Acceptance Criteria
1. Critical thresholds are defined per subsystem and configurable at runtime.
2. A single subsystem reaching critical state triggers RTB within 100 ms of detection.
3. Multiple subsystems in warning state simultaneously (≥2) escalates to RTB.
4. The RTB command is placed on the flight controller command queue with a critical priority flag.
5. The RTB decision, triggering readings, and timestamp are logged as a single atomic event entry.

### Dependencies
- US-02 (requires monitoring data)
- US-03 (warning state must be tracked for escalation logic)

---

## US-05: Degraded Mode Operation

**Priority:** CRITICAL  
**Stakeholder:** UAV Autopilot System

> As the UAV autopilot system, I need the health guardian to tolerate a single subsystem fault without commanding RTB so that I can continue the mission when the vehicle is still capable of safe flight.

### Acceptance Criteria
1. A single subsystem reporting a transient fault (non-critical) does not trigger RTB.
2. The faulted subsystem is marked degraded; remaining subsystems continue monitoring at normal rates.
3. The degraded state is reported to GCS via MAVLink with the affected subsystem identifier.
4. If the faulted subsystem recovers (readings return to normal range), it automatically transitions back to healthy state.
5. If a second subsystem enters fault state while the first is degraded, RTB is triggered per US-04 escalation rules.

### Dependencies
- US-03 (warning/fault state tracking)
- US-04 (escalation rules)

---

## US-06: RTB Command Interface

**Priority:** HIGH  
**Stakeholder:** UAV Autopilot System

> As the UAV autopilot system, I need to receive RTB commands from the health guardian through a well-defined interface so that I can execute the return sequence using my navigation subsystem.

### Acceptance Criteria
1. The RTB command is delivered via a FreeRTOS queue with a defined message structure (command type, urgency level, triggering reason, timestamp).
2. The command interface is abstracted behind a C++ interface class to decouple health guardian from autopilot implementation.
3. The autopilot acknowledges receipt of the RTB command; if no acknowledgement within 200 ms, the health guardian re-sends once.
4. The interface supports future extension for other protective commands (e.g., altitude reduction, loiter) without breaking changes.

### Dependencies
- US-04 (RTB decision triggers this interface)

---

## US-07: MAVLink Telemetry Broadcasting

**Priority:** HIGH  
**Stakeholder:** GCS Operator

> As a GCS operator, I need to receive periodic health status and immediate alerts via MAVLink so that I can monitor vehicle health in real time and make informed decisions about mission continuation.

### Acceptance Criteria
1. A periodic heartbeat message (default: 1 Hz) includes summary health status for all three subsystems.
2. State transition events (normal → warning → critical → degraded) generate immediate MAVLink messages.
3. Messages conform to MAVLink v2 framing with correct sequence numbers, system ID, and component ID.
4. If the MAVLink transmit queue is full, the oldest non-critical message is dropped (critical messages are never dropped).
5. Message construction is decoupled from transport — the health guardian enqueues formatted messages for a separate transmit task.

### Dependencies
- US-02 (requires monitoring data to report)
- US-01 (system/component IDs set at startup)

---

## US-08: Runtime Threshold Configuration

**Priority:** MEDIUM  
**Stakeholder:** GCS Operator

> As a GCS operator, I need to update warning and critical thresholds at runtime so that I can adapt the health guardian's sensitivity to changing mission conditions without requiring a reboot.

### Acceptance Criteria
1. Threshold values can be updated per subsystem via a configuration command interface.
2. New threshold values take effect on the next sample cycle (no restart required).
3. Threshold updates are protected by a mutex to prevent partial writes from corrupting active thresholds.
4. Each threshold change is logged with old value, new value, and timestamp.
5. Invalid threshold values (e.g., warning > critical, negative voltage) are rejected with an error response.

### Dependencies
- US-02 (thresholds are consumed by the monitoring tasks)
- US-03 (warning evaluation uses these thresholds)

---

## US-09: Tamper-Proof Flash Event Logging

**Priority:** HIGH  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need to log all health events to flash storage with CRC32 integrity protection so that a complete and verifiable record exists for every flight regardless of communication link status.

### Acceptance Criteria
1. Every state transition, threshold breach, RTB decision, and startup/shutdown event is written to flash.
2. Each log entry includes: microsecond timestamp, event type, subsystem ID, reading value(s), and CRC32 computed over all preceding fields.
3. Log writes are append-only — existing entries cannot be modified or overwritten.
4. The logging task operates at a lower priority than monitoring tasks to avoid impacting real-time performance.
5. If flash write fails, the failure is reported via MAVLink and the system continues operating (logging failure does not trigger RTB).

### Dependencies
- US-01 (flash HAL initialized at startup)
- US-02, US-03, US-04 (generate the events to be logged)

---

## US-10: Post-Mission Log Retrieval and Verification

**Priority:** HIGH  
**Stakeholder:** Maintenance Engineer

> As a maintenance engineer, I need to retrieve flight logs after mission completion and verify their integrity so that I can diagnose issues, confirm system behavior, and provide evidence for airworthiness reviews.

### Acceptance Criteria
1. Logs are retrievable as a sequential stream of entries from flash storage.
2. Each entry's CRC32 can be independently recomputed and verified against the stored value.
3. A corrupted entry (CRC mismatch) is flagged but does not prevent reading subsequent entries.
4. Log entries include sufficient context (event type, subsystem, values, timestamps) to reconstruct the health timeline without access to source code.
5. A log summary report can enumerate: total entries, time span, count of warnings/criticals/RTB events, and any integrity failures.

### Dependencies
- US-09 (logs must exist in flash to be retrieved)

---

## US-11: Safety Audit and Compliance Evidence

**Priority:** MEDIUM  
**Stakeholder:** Program Manager / Safety Officer

> As a program manager, I need documented evidence that the health guardian detects and responds to all defined failure modes so that I can demonstrate compliance with safety requirements during certification reviews.

### Acceptance Criteria
1. Each threshold type (warning, critical) for each subsystem has a corresponding test case that demonstrates detection and correct response.
2. The degraded mode behavior (single fault tolerance) is verified by test cases showing continued operation with one subsystem faulted.
3. RTB escalation logic is covered by test cases for both single-critical and multi-warning triggers.
4. All test cases pass in the POSIX simulation environment using Catch2 and produce a machine-readable results report.
5. Log integrity verification is demonstrated end-to-end: write events, read back, verify CRC32 on each entry.

### Dependencies
- US-01 through US-10 (this story validates the behavior of all others)

---

## US-12: Watchdog Recovery After Fault

**Priority:** HIGH  
**Stakeholder:** UAV Vehicle (autonomous agent)

> As the UAV vehicle, I need the health guardian to recover from task-level faults via a watchdog mechanism so that a software error in one monitoring task does not permanently disable my self-protection capability.

### Acceptance Criteria
1. Each monitoring task feeds a software watchdog (FreeRTOS timer or heartbeat counter) at its expected sample rate.
2. If a task fails to feed the watchdog within 2× its sample period, the watchdog triggers a task restart.
3. A watchdog-triggered restart is logged as a critical event with the faulted task identifier and timestamp.
4. After restart, the recovered task resumes monitoring from the current state (no stale data carried over).
5. If the same task faults 3 times within 60 seconds, it is permanently disabled and the system escalates to RTB.

### Dependencies
- US-01 (watchdog configured at startup)
- US-02 (watchdog monitors the monitoring tasks)
- US-04 (repeated faults escalate to RTB)

---

## Dependency Graph

```
US-01 (Startup)
 ├── US-02 (Continuous Monitoring)
 │    ├── US-03 (Warning Threshold)
 │    │    ├── US-04 (Critical / RTB Trigger)
 │    │    │    ├── US-05 (Degraded Mode)
 │    │    │    └── US-06 (RTB Command Interface)
 │    │    └── US-08 (Runtime Config)
 │    ├── US-07 (MAVLink Telemetry)
 │    └── US-09 (Flash Logging)
 │         └── US-10 (Log Retrieval)
 ├── US-12 (Watchdog Recovery)
 └── US-11 (Safety Audit — validates all)
```

---

## Stakeholder Coverage Summary

| Stakeholder | Stories |
|-------------|---------|
| UAV Vehicle (autonomous agent) | US-01, US-02, US-03, US-04, US-09, US-12 |
| UAV Autopilot System | US-05, US-06 |
| GCS Operator | US-07, US-08 |
| Maintenance Engineer | US-10 |
| Program Manager / Safety Officer | US-11 |
