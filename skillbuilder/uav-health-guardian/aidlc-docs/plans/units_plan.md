# Plan: System Unit Decomposition

## Objective
Decompose the UAV Health Guardian into exactly 5 independent, testable units with defined interfaces, dependencies, and build sequence.

## Approach

### Step 1: Define Unit Boundaries
- [x] Document each unit's single responsibility statement
- [x] Identify inputs (what data/events flow in) and outputs (what data/events flow out)
- [x] Verify each unit is independently testable via interface injection

### Step 2: Specify Interface Contracts
- [x] Define ISensorHAL pure abstract interface (Unit 1)
- [x] Define HealthSnapshot data structure and accessor contract (Unit 2)
- [x] Define RTB state machine states, transitions, and trigger rules (Unit 3)
- [x] Define ITransport interface and MAVLink message contracts (Unit 4)
- [x] Define LogEntry structure and circular buffer contract (Unit 5)

### Step 3: Map Dependencies
- [x] Identify which units depend on which interfaces (not implementations)
- [x] Verify no circular dependencies exist
- [x] Draw dependency graph showing the layered architecture

### Step 4: Determine Build Sequence
- [x] Order units from least to most dependent
- [x] Assign complexity estimates based on: state management, concurrency, external protocol knowledge
- [x] Verify that each unit in the sequence can be built and tested with only previously-completed units (or mocks)

### Step 5: Cross-Reference to User Stories
- [x] Map each unit to the user stories it enables
- [x] Confirm full coverage — all 12 user stories are addressed by at least one unit

### Step 6: Write and Log
- [x] Write completed decomposition to `aidlc-docs/story-artifacts/units.md`
- [x] Log prompt to `aidlc-docs/prompts.md`

## Expected Output Structure

```
units.md
├── Overview table (unit, responsibility, complexity)
├── Unit 1: Sensor HAL (detail)
├── Unit 2: Health Monitor (detail)
├── Unit 3: RTB Manager (detail)
├── Unit 4: MAVLink Reporter (detail)
├── Unit 5: Flight Event Logger (detail)
├── Dependency Graph (ASCII)
├── Recommended Build Sequence
└── User Story Traceability Matrix
```

## Key Design Decisions
- Dependencies flow inward: Units 3, 4, 5 depend on Unit 2; Unit 2 depends on Unit 1
- All inter-unit coupling is via abstract interfaces → enables mock injection for testing
- Unit 5 (Logger) is an observer — it receives events but nothing depends on its output at runtime
- Build sequence follows dependency order: 1 → 2 → 3/4/5 (last three are independent of each other)
