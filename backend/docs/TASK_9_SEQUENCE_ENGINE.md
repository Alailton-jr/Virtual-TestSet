# Task 9: Sequence Engine - Implementation Summary

## Overview
The Sequence Engine enables automated multi-state test scenarios for Virtual TestSet. It manages sequential execution of test states with configurable phasor values and supports both time-based and GOOSE-trip-based transitions.

## Status: ✅ 80% Complete

### Completed (This Session)
- ✅ Core API design and implementation
- ✅ State machine execution logic
- ✅ Time-based transitions
- ✅ GOOSE-trip transitions with timeout
- ✅ Pause/resume functionality
- ✅ Progress callbacks
- ✅ Phasor update callbacks
- ✅ Thread-safe atomic operations
- ✅ Comprehensive unit tests (20 tests, 100% pass)
- ✅ CMake build integration
- ✅ Security scans (Snyk - 0 issues)

### Pending (20% Remaining)
- ⏳ REST API endpoints (`/api/v1/sequences/run`, `/stop`, `/status`)
- ⏳ Integration with SV Publisher Manager
- ⏳ WebSocket progress reporting
- ⏳ End-to-end integration tests

## Architecture

### Core Components

#### 1. **SequenceEngine Class**
Main engine that manages sequence execution.

**Location**: `backend/src/sequence/include/sequence_engine.hpp`

**Key Methods**:
```cpp
bool start(const Sequence& sequence);           // Start sequence execution
void stop();                                     // Stop execution
void pause();                                    // Pause execution
void resume();                                   // Resume from pause
SequenceStatus getStatus() const;               // Get current status
int getCurrentStateIndex() const;               // Get active state index
double getStateElapsedTime() const;             // Time in current state
double getTotalElapsedTime() const;             // Total elapsed time
void setProgressCallback(ProgressCallback cb);  // Set progress callback
void setPhasorUpdateCallback(PhasorUpdateCallback cb); // Set phasor callback
```

**Thread Safety**: Uses atomic operations for all shared state (no mutexes required).

#### 2. **Data Structures**

**ChannelPhasor**:
```cpp
struct ChannelPhasor {
    double mag;       // Magnitude (RMS or peak depending on context)
    double angleDeg;  // Angle in degrees
};
```

**StreamPhasorState**:
```cpp
struct StreamPhasorState {
    double freq;                                    // Frequency in Hz
    std::map<std::string, ChannelPhasor> channels; // Channel ID → phasor
};
```

**SequenceState**:
```cpp
struct SequenceState {
    std::string name;                                     // State name
    double durationSec;                                   // Duration/timeout
    StateTransition transition;                           // Transition type
    std::map<std::string, StreamPhasorState> phasors;    // Stream ID → state
};
```

**Sequence**:
```cpp
struct Sequence {
    std::vector<std::string> activeStreams;  // Active stream IDs
    std::vector<SequenceState> states;       // Sequence states
};
```

#### 3. **Transition Types**

**TIME**: Duration-based transition
- Waits for specified `durationSec` to elapse
- Polls every 50ms for pause/stop requests
- Advances automatically when time expires

**GOOSE_TRIP**: Event-based transition
- Monitors `GLOBAL_TRIP_FLAG` (set by GOOSE subscriber)
- Polls every 50ms until flag is set or timeout
- Uses `durationSec` as maximum wait time
- Advances when trip occurs or timeout expires

#### 4. **Status States**

```cpp
enum class SequenceStatus {
    IDLE,       // Not running
    RUNNING,    // Executing states
    PAUSED,     // Paused by user
    COMPLETED,  // All states finished
    STOPPED,    // Stopped by user
    ERROR       // Error occurred
};
```

### Implementation Details

#### Execution Flow
1. **Start**:
   - Validate sequence (non-empty states, active streams)
   - Clear GOOSE trip flag
   - Spawn background thread
   - Set status to RUNNING

2. **State Execution** (background thread):
   ```
   For each state:
     - Check for stop/pause requests
     - Report progress (callback)
     - Apply phasor state (callback)
     - Wait for transition:
       - TIME: Poll every 50ms until duration expires
       - GOOSE_TRIP: Poll trip flag every 50ms or timeout
     - Handle pause (wait until resume)
     - Advance to next state
   
   Set status to COMPLETED
   ```

3. **Pause/Resume**:
   - Pause: Set atomic flag, background thread enters wait loop
   - Resume: Clear flag, execution continues from same state
   - Elapsed time tracking pauses during pause

4. **Stop**:
   - Set atomic stop flag
   - Background thread detects on next poll and exits
   - Join thread in `stop()` method

#### Timing Accuracy
- Uses `std::chrono::steady_clock` (monotonic)
- 50ms poll interval balances responsiveness and CPU usage
- State elapsed time measured from state start to current time
- Total elapsed time accumulated across all states

#### Callback Design

**ProgressCallback**:
```cpp
std::function<void(
    size_t currentState,     // 0-based index
    size_t totalStates,      // Total state count
    const std::string& name, // State name
    double elapsed,          // Total elapsed seconds
    const std::string& msg   // Status message
)>
```

**PhasorUpdateCallback**:
```cpp
std::function<void(
    const std::string& streamId,      // Stream ID
    const StreamPhasorState& state    // Phasor values
)>
```

### Integration Points

#### 1. **GOOSE Subscriber** (Task 8)
- Reads `GLOBAL_TRIP_FLAG` via `isTripFlagSet()`
- Clears flag on sequence start via `clearTripFlag()`
- Location: `src/goose/include/global_flags.hpp`

#### 2. **SV Publisher Manager** (Task 5) - PENDING
- Will receive phasor updates via `PhasorUpdateCallback`
- Each state application calls callback for all active streams
- Manager updates published SV values in real-time

#### 3. **HTTP Server** (Task 4) - PENDING
- REST endpoints:
  - `POST /api/v1/sequences/run`: Parse JSON, start sequence
  - `POST /api/v1/sequences/stop`: Stop current sequence
  - `GET /api/v1/sequences/status`: Current status, state, elapsed time

#### 4. **WebSocket Server** (Task 4) - PENDING
- Emits `sequenceProgress` messages via `ProgressCallback`
- Format:
  ```json
  {
    "type": "sequenceProgress",
    "state": 1,
    "total": 3,
    "name": "Fault",
    "elapsed": 2.5,
    "message": "State 'Fault' started"
  }
  ```

## Testing

### Unit Tests
**Location**: `backend/tests/test_sequence_engine.cpp`

**Coverage**: 20 tests, all passing
- ✅ Basic sequence execution
- ✅ Multiple states
- ✅ Time transitions
- ✅ GOOSE-trip transitions (with and without timeout)
- ✅ Pause/resume
- ✅ Stop mid-execution
- ✅ Multiple streams
- ✅ Multiple channels per stream
- ✅ State timing accuracy
- ✅ Current state tracking
- ✅ Validation (empty sequences, no streams)
- ✅ Callback data correctness
- ✅ Edge cases (single state, long sequences, rapid trip)

**Run Tests**:
```bash
cd backend
./build/vts_tests --gtest_filter="SequenceEngineTest.*"
```

**Results**: 20/20 tests pass (8.2 seconds total)

### Security Scans
✅ **Snyk Code Scan**: 0 issues found
- Scanned: `src/sequence/` (all files)
- Scanned: `tests/test_sequence_engine.cpp`

## Build System

### CMake Configuration
**File**: `backend/src/sequence/CMakeLists.txt`

```cmake
add_library(sequence STATIC
    src/sequence_engine.cpp
)

target_include_directories(sequence PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sequence PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/platform/include
    ${CMAKE_SOURCE_DIR}/src/tools/include
)

target_link_libraries(sequence
    vts_core
    pthread
)
```

**Build**:
```bash
cd backend
cmake --build build
```

**Output**: `build/src/sequence/libsequence.a`

## Example Usage

### Basic Sequence
```cpp
#include "sequence_engine.hpp"
#include <iostream>

using namespace vts::sequence;

int main() {
    SequenceEngine engine;
    
    // Set up callbacks
    engine.setProgressCallback([](size_t current, size_t total,
                                   const std::string& name, double elapsed,
                                   const std::string& msg) {
        std::cout << "State " << (current + 1) << "/" << total 
                  << ": " << name << " - " << msg << std::endl;
    });
    
    engine.setPhasorUpdateCallback([](const std::string& streamId,
                                      const StreamPhasorState& state) {
        std::cout << "Update " << streamId 
                  << " to " << state.freq << " Hz" << std::endl;
    });
    
    // Create sequence
    Sequence seq;
    seq.activeStreams = {"SV1"};
    
    // State 1: Pre-Fault (2 seconds)
    SequenceState preFault;
    preFault.name = "Pre-Fault";
    preFault.durationSec = 2.0;
    preFault.transition = StateTransition(TransitionType::TIME);
    preFault.phasors["SV1"].freq = 60.0;
    preFault.phasors["SV1"].channels["V-A"] = ChannelPhasor(69000.0, 0.0);
    seq.states.push_back(preFault);
    
    // State 2: Fault (wait for GOOSE trip, max 5 seconds)
    SequenceState fault;
    fault.name = "Fault";
    fault.durationSec = 5.0;
    fault.transition = StateTransition(TransitionType::GOOSE_TRIP);
    fault.phasors["SV1"].freq = 60.5;
    fault.phasors["SV1"].channels["V-A"] = ChannelPhasor(120000.0, -30.0);
    seq.states.push_back(fault);
    
    // State 3: Post-Trip (1 second)
    SequenceState postTrip;
    postTrip.name = "Post-Trip";
    postTrip.durationSec = 1.0;
    postTrip.transition = StateTransition(TransitionType::TIME);
    postTrip.phasors["SV1"].freq = 60.0;
    postTrip.phasors["SV1"].channels["V-A"] = ChannelPhasor(69000.0, 0.0);
    seq.states.push_back(postTrip);
    
    // Start sequence
    if (!engine.start(seq)) {
        std::cerr << "Failed to start: " << engine.getLastError() << std::endl;
        return 1;
    }
    
    // Monitor status
    while (engine.getStatus() == SequenceStatus::RUNNING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Sequence completed!" << std::endl;
    return 0;
}
```

### JSON Sequence Format (for REST API)
```json
{
  "activeStreams": ["SV1", "SV2"],
  "states": [
    {
      "name": "Pre-Fault",
      "durationSec": 2.0,
      "transition": { "type": "time" },
      "phasors": {
        "SV1": {
          "freq": 60.0,
          "channels": {
            "V-A": { "mag": 69000.0, "angleDeg": 0.0 },
            "V-B": { "mag": 69000.0, "angleDeg": -120.0 },
            "V-C": { "mag": 69000.0, "angleDeg": 120.0 }
          }
        }
      }
    },
    {
      "name": "Fault",
      "durationSec": 5.0,
      "transition": { "type": "goose_trip" },
      "phasors": {
        "SV1": {
          "freq": 60.5,
          "channels": {
            "V-A": { "mag": 120000.0, "angleDeg": -30.0 },
            "V-B": { "mag": 120000.0, "angleDeg": -150.0 },
            "V-C": { "mag": 120000.0, "angleDeg": 90.0 }
          }
        }
      }
    }
  ]
}
```

## Performance Characteristics

### Memory Usage
- **Sequence Storage**: O(states × streams × channels)
- **Runtime Overhead**: < 1 KB (atomic flags, timing data)
- **Thread Count**: 1 background thread

### CPU Usage
- **Polling**: 50ms interval (~20 Hz)
- **Per-Poll Work**: ~1-2 μs (atomic checks)
- **Callback Overhead**: Depends on callback implementation

### Timing Accuracy
- **Resolution**: ±50ms (poll interval)
- **Drift**: None (uses monotonic clock)
- **Latency**: < 100ms from trip flag to state transition

## Dependencies

### Internal (Virtual TestSet)
- `vts_core`: Core utilities
- `platform`: Platform abstractions
- `tools/logger.hpp`: Logging macros
- `goose/global_flags.hpp`: Trip flag access

### External (Standard Library)
- `<atomic>`: Thread-safe flags
- `<thread>`: Background execution
- `<chrono>`: Timing
- `<functional>`: Callbacks
- `<map>`, `<vector>`, `<string>`: Data structures

### Build Dependencies
- CMake 3.14+
- C++17 compiler
- pthread (POSIX threads)

## Next Steps (20% Remaining)

### 1. REST API Endpoints (HIGH PRIORITY)
**File**: `backend/src/api/http_server.cpp`

**Implementation**:
```cpp
// POST /api/v1/sequences/run
void handleSequenceRun(const crow::request& req, crow::response& res) {
    auto json = nlohmann::json::parse(req.body);
    
    // Parse JSON to Sequence
    Sequence seq;
    seq.activeStreams = json["activeStreams"];
    
    for (const auto& stateJson : json["states"]) {
        SequenceState state;
        state.name = stateJson["name"];
        state.durationSec = stateJson["durationSec"];
        // ... parse transition and phasors ...
        seq.states.push_back(state);
    }
    
    // Start sequence
    if (!g_sequenceEngine->start(seq)) {
        res.code = 400;
        res.write(json_error(g_sequenceEngine->getLastError()));
        return;
    }
    
    res.code = 200;
    res.write(json_success("Sequence started"));
}

// POST /api/v1/sequences/stop
void handleSequenceStop(const crow::request& req, crow::response& res) {
    g_sequenceEngine->stop();
    res.code = 200;
    res.write(json_success("Sequence stopped"));
}

// GET /api/v1/sequences/status
void handleSequenceStatus(const crow::request& req, crow::response& res) {
    nlohmann::json status;
    status["status"] = statusToString(g_sequenceEngine->getStatus());
    status["currentState"] = g_sequenceEngine->getCurrentStateIndex();
    status["stateElapsed"] = g_sequenceEngine->getStateElapsedTime();
    status["totalElapsed"] = g_sequenceEngine->getTotalElapsedTime();
    
    res.code = 200;
    res.write(status.dump());
}
```

**Effort**: 2-3 hours

### 2. SV Publisher Manager Integration (HIGH PRIORITY)
**File**: `backend/src/sampledValue/src/sv_publisher_manager.cpp`

**Implementation**:
```cpp
// Add to SVPublisherManager class
void SVPublisherManager::updateStreamPhasors(
    const std::string& streamId,
    const vts::sequence::StreamPhasorState& state)
{
    auto it = publishers.find(streamId);
    if (it == publishers.end()) {
        LOG_WARN("Stream '{}' not found", streamId);
        return;
    }
    
    // Update frequency
    it->second->setFrequency(state.freq);
    
    // Update each channel
    for (const auto& [channelId, phasor] : state.channels) {
        it->second->updateChannel(channelId, phasor.mag, phasor.angleDeg);
    }
}

// Wire callback in main.cpp
g_sequenceEngine->setPhasorUpdateCallback(
    [](const std::string& streamId, const StreamPhasorState& state) {
        g_svPublisherManager->updateStreamPhasors(streamId, state);
    }
);
```

**Effort**: 1-2 hours

### 3. WebSocket Progress Reporting (MEDIUM PRIORITY)
**File**: `backend/src/api/websocket_server.cpp`

**Implementation**:
```cpp
// Wire callback in main.cpp
g_sequenceEngine->setProgressCallback(
    [](size_t current, size_t total, const std::string& name,
       double elapsed, const std::string& msg) {
        nlohmann::json progress;
        progress["type"] = "sequenceProgress";
        progress["state"] = current;
        progress["total"] = total;
        progress["name"] = name;
        progress["elapsed"] = elapsed;
        progress["message"] = msg;
        
        g_websocketServer->broadcast(progress.dump());
    }
);
```

**Effort**: 1 hour

### 4. Integration Tests (MEDIUM PRIORITY)
**File**: `backend/tests/test_sequence_integration.cpp`

**Tests**:
- Sequence → SV Publisher Manager → SV transmission
- Sequence → WebSocket progress emission
- REST API → Sequence engine control
- GOOSE trip → Sequence state transition

**Effort**: 2-3 hours

### 5. Documentation (LOW PRIORITY)
- User guide with examples
- API reference documentation
- Integration guide for frontend developers

**Effort**: 1-2 hours

## Known Limitations

1. **Single Active Sequence**: Only one sequence can run at a time
   - **Workaround**: Check status before starting
   - **Future**: Support queued sequences

2. **No State Callbacks**: Cannot execute custom code per state
   - **Workaround**: Use phasor callback for side effects
   - **Future**: Add state enter/exit callbacks

3. **No Conditional Transitions**: All transitions are linear
   - **Workaround**: Use multiple simple sequences
   - **Future**: Add branching based on conditions

4. **No Real-Time Guarantees**: Polling-based with 50ms resolution
   - **Workaround**: Acceptable for power system testing
   - **Future**: Consider real-time thread if needed

## Conclusion

Task 9 (Sequence Engine) is **80% complete**. The core state machine is fully implemented, tested, and secure. Remaining work focuses on integration with existing systems (REST API, SV Publisher Manager, WebSocket) and end-to-end testing.

**Total Effort Remaining**: ~7-11 hours (1 day)

**Test Results**: 114/114 tests pass (20 new sequence tests)

**Security**: 0 vulnerabilities (Snyk scan)

**Files Created**:
- `backend/src/sequence/include/sequence_engine.hpp` (220 lines)
- `backend/src/sequence/src/sequence_engine.cpp` (330 lines)
- `backend/src/sequence/CMakeLists.txt` (22 lines)
- `backend/tests/test_sequence_engine.cpp` (520 lines)
- `backend/docs/TASK_9_SEQUENCE_ENGINE.md` (this file)

**Total Lines of Code**: ~1,100 lines

---

**Last Updated**: Session with token budget limit
**Next Action**: Implement REST API endpoints for sequence control
