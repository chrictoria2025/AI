# Plan: Domain Model Construction

## Objective
Produce a comprehensive domain model for the UAV Flight Computer Health Guardian that serves as the authoritative reference for the construction phase — covering component relationships, data flow, data structures, FreeRTOS task layout, and key architectural decisions.

## Approach

### Step 1: Component Diagram
- [x] Create Mermaid diagram showing all 5 units (Sensor HAL, Health Monitor, RTB Manager, MAVLink Reporter, Flight Event Logger)
- [x] Show interface relationships (ISensorHAL, IHealthMonitor, IRTBManager, ITransport, IFlashStorage)
- [x] Include external actors (GCS, Autopilot, Flash, Hardware Sensors)

### Step 2: Data Flow
- [x] Document the primary data flow path: HAL → Health Monitor → consumers
- [x] Show event-driven flows (state transitions → Logger, Reporter)
- [x] Identify FreeRTOS primitives used at each boundary (queues, mutexes, direct calls)

### Step 3: Key Data Structures
- [x] Define SensorReading struct
- [x] Define AlertLevel enum class
- [x] Define SubsystemType enum class
- [x] Define HealthSnapshot struct
- [x] Define RTBState enum class
- [x] Define EventType enum class
- [x] Define LogEntry struct (with CRC32 field and message buffer)
- [x] Ensure all structs are fixed-size and suitable for embedded use (no dynamic allocation)

### Step 4: FreeRTOS Task Layout
- [x] Document task table with priority, stack size, and period
- [x] Justify priority assignments relative to real-time requirements
- [x] List shared resources and their protection mechanisms (mutexes, queues)

### Step 5: Key Design Decisions
- [x] Document 3-5 architectural decisions with rationale
- [x] Cover: HAL abstraction strategy, concurrency model, event distribution, memory model, testability approach

### Step 6: Write and Log
- [x] Write completed domain model to `aidlc-docs/design-artifacts/domain_model.md`
- [x] Log prompt to `aidlc-docs/prompts.md`

## Constraints
- All data structures must be C++17 compatible
- No dynamic memory allocation after initialization
- Fixed-size types throughout (suitable for Cortex-M)
- Mermaid diagrams for visual representation
