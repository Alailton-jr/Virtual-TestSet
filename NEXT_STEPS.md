# What to Do Next

**TL;DR**: You have the foundation ready. Now implement the SV Publisher Manager, then the Phasor Synthesizer, then wire them up to the HTTP server. After that, you can test the first end-to-end workflow.

---

## 🎯 Immediate Next Action

### **Step 1: Implement SV Publisher Manager** (Est: 4-6 hours)

This is the **most critical component** - everything else depends on it.

#### 1.1 Create the Manager Header

**File**: `backend/src/core/include/sv_publisher_manager.hpp`

```cpp
#pragma once
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include "sv_publisher_instance.hpp"

class SVPublisherManager {
public:
    SVPublisherManager();
    ~SVPublisherManager();

    // CRUD operations
    std::string createStream(const nlohmann::json& config);
    void updateStream(const std::string& streamId, const nlohmann::json& config);
    void deleteStream(const std::string& streamId);
    nlohmann::json listStreams() const;
    nlohmann::json getStream(const std::string& streamId) const;

    // Control
    void startStream(const std::string& streamId);
    void stopStream(const std::string& streamId);
    void startAll();
    void stopAll();

    // Updates
    void updatePhasors(const std::string& streamId, const nlohmann::json& phasorData);
    void updateHarmonics(const std::string& streamId, const nlohmann::json& harmonicsData);

    // High-resolution tick
    void tickAll();

    // Getters
    std::shared_ptr<SVPublisherInstance> getInstance(const std::string& streamId);

private:
    std::map<std::string, std::shared_ptr<SVPublisherInstance>> streams_;
    mutable std::mutex mutex_;

    std::string generateId() const;
};
```

#### 1.2 Create the Instance Header

**File**: `backend/src/core/include/sv_publisher_instance.hpp`

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

enum class DataSource {
    MANUAL,
    COMTRADE,
    CSV
};

struct SVConfig {
    std::string appId;
    std::string macDst;
    std::string macSrc;
    uint16_t vlanId;
    uint8_t vlanPrio;
    std::string svId;
    std::string dstAddress;
    double nominalFreq;
    uint32_t sampleRate;
    DataSource dataSource;
    std::string filePath; // for COMTRADE/CSV
};

struct Phasor {
    double magnitude;
    double angle;
};

class SVPublisherInstance {
public:
    SVPublisherInstance(const std::string& id, const SVConfig& config);
    ~SVPublisherInstance();

    // Control
    void start();
    void stop();
    bool isRunning() const { return running_; }

    // Configuration
    const SVConfig& getConfig() const { return config_; }
    void setConfig(const SVConfig& config);

    // Phasor updates (for manual mode)
    void setPhasors(const std::vector<Phasor>& phasors);
    const std::vector<Phasor>& getPhasors() const { return phasors_; }

    // Harmonics (for manual mode)
    void setHarmonics(const nlohmann::json& harmonics);

    // Tick function
    void tick();

    // Serialization
    nlohmann::json toJson() const;

    const std::string& getId() const { return id_; }

private:
    std::string id_;
    SVConfig config_;
    bool running_;
    std::vector<Phasor> phasors_;
    nlohmann::json harmonics_;
    uint32_t sampleCounter_;
    int rawSocket_;

    void sendSVPacket();
    std::vector<int16_t> generateSamples();
};
```

#### 1.3 Implement the Manager

**File**: `backend/src/core/src/sv_publisher_manager.cpp`

Key methods to implement:
- `createStream()`: Parse JSON config, create SVPublisherInstance, add to map, return UUID
- `updateStream()`: Find instance, update config
- `deleteStream()`: Stop if running, remove from map
- `listStreams()`: Return JSON array of all stream summaries
- `startStream()`: Find instance, call `start()`
- `updatePhasors()`: Find instance, call `setPhasors()`
- `tickAll()`: Iterate map, call `tick()` on each running instance

#### 1.4 Implement the Instance

**File**: `backend/src/core/src/sv_publisher_instance.cpp`

Key methods to implement:
- Constructor: Initialize config, open raw socket, set up SV frame template
- `start()`: Set running flag
- `stop()`: Clear running flag
- `tick()`: If running, increment sample counter, generate samples, send packet
- `sendSVPacket()`: Build Ethernet + VLAN + SV packet, send via raw socket
- `generateSamples()`: Based on data source:
  - MANUAL: Call phasor synthesizer
  - COMTRADE: Read from parsed file
  - CSV: Read from CSV parser

#### 1.5 Create CMakeLists.txt

**File**: `backend/src/core/CMakeLists.txt`

```cmake
add_library(vts_core
    src/sv_publisher_manager.cpp
    src/sv_publisher_instance.cpp
)

target_include_directories(vts_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/json/include
)

target_link_libraries(vts_core
    PRIVATE
        pthread
)
```

#### 1.6 Update Main CMakeLists.txt

Add to `backend/CMakeLists.txt`:

```cmake
add_subdirectory(src/core)
```

And link in the main executable:

```cmake
target_link_libraries(Main PRIVATE vts_core vts_api)
```

---

### **Step 2: Implement Phasor Synthesizer** (Est: 3-4 hours)

#### 2.1 Create Header

**File**: `backend/src/synth/include/phasor_synth.hpp`

```cpp
#pragma once
#include <vector>
#include <cstdint>

struct Phasor {
    double magnitude;
    double angle;
};

struct HarmonicComponent {
    int order;
    double magnitude;
    double angle;
};

class PhasorSynth {
public:
    // Generate samples from phasor (fundamental only)
    static std::vector<int16_t> synthesize(
        const Phasor& phasor,
        double frequency,
        uint32_t sampleRate,
        uint32_t startSample,
        uint32_t numSamples
    );

    // Generate samples with harmonics
    static std::vector<int16_t> synthesizeWithHarmonics(
        const Phasor& fundamental,
        const std::vector<HarmonicComponent>& harmonics,
        double frequency,
        uint32_t sampleRate,
        uint32_t startSample,
        uint32_t numSamples
    );

private:
    static constexpr double SCALE_FACTOR = 3276.7; // for ±10V to 16-bit
};
```

#### 2.2 Implement

**File**: `backend/src/synth/src/phasor_synth.cpp`

Algorithm (fundamental only):

```
For sample i:
    t = (startSample + i) / sampleRate
    phase = 2π * frequency * t + angle
    value = magnitude * √2 * sin(phase)
    scaled = (int16_t)(value * SCALE_FACTOR)
```

With harmonics:

```
For sample i:
    value = 0
    For each harmonic (including fundamental):
        t = (startSample + i) / sampleRate
        phase = 2π * n * frequency * t + angle_n
        value += magnitude_n * √2 * sin(phase)
    scaled = (int16_t)(value * SCALE_FACTOR)
```

#### 2.3 Create CMakeLists.txt

**File**: `backend/src/synth/CMakeLists.txt`

```cmake
add_library(vts_synth
    src/phasor_synth.cpp
)

target_include_directories(vts_synth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

---

### **Step 3: Wire Up HTTP Server** (Est: 1-2 hours)

#### 3.1 Update main.cpp

**File**: `backend/src/main/src/main.cpp`

```cpp
#include "http_server.hpp"
#include "sv_publisher_manager.hpp"
#include <memory>

int main() {
    auto svManager = std::make_shared<SVPublisherManager>();
    
    HTTPServer server(8080);
    server.setSVPublisherManager(svManager);
    
    server.start();
    
    // Main tick loop
    while (true) {
        svManager->tickAll();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    return 0;
}
```

#### 3.2 Update HTTPServer Implementation

In `backend/src/api/src/http_server.cpp`, replace TODOs:

**createStream handler**:
```cpp
auto streamId = svManager_->createStream(body);
json response = {{"id", streamId}};
sendJsonResponse(res, response, 201);
```

**listStreams handler**:
```cpp
auto streams = svManager_->listStreams();
sendJsonResponse(res, streams);
```

**startStream handler**:
```cpp
svManager_->startStream(streamId);
sendJsonResponse(res, {{"status", "started"}});
```

**updatePhasors handler**:
```cpp
svManager_->updatePhasors(streamId, body);
sendJsonResponse(res, {{"status", "updated"}});
```

---

### **Step 4: Build and Test** (Est: 1 hour)

#### 4.1 Build

```bash
cd backend
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Expected: Clean build with no errors

#### 4.2 Run

```bash
sudo ./build/Main  # sudo required for raw socket
```

Expected output:
```
HTTP server starting on port 8080...
HTTP server running!
```

#### 4.3 Test with curl

**Health check**:
```bash
curl http://localhost:8080/api/v1/health
```

Expected: `{"status": "ok"}`

**Create stream**:
```bash
curl -X POST http://localhost:8080/api/v1/streams \
  -H "Content-Type: application/json" \
  -d '{
    "appId": "0x4000",
    "macDst": "01:0C:CD:04:00:00",
    "macSrc": "AA:BB:CC:DD:EE:01",
    "vlanId": 100,
    "vlanPrio": 4,
    "svId": "IED1MU01",
    "dstAddress": "",
    "nominalFreq": 60.0,
    "sampleRate": 4800,
    "dataSource": "MANUAL"
  }'
```

Expected: `{"id": "<some-uuid>"}`

**Update phasors**:
```bash
curl -X PUT http://localhost:8080/api/v1/streams/<stream-id>/phasors \
  -H "Content-Type: application/json" \
  -d '{
    "phasors": [
      {"magnitude": 120.0, "angle": 0.0},
      {"magnitude": 120.0, "angle": -120.0},
      {"magnitude": 120.0, "angle": 120.0}
    ]
  }'
```

Expected: `{"status": "updated"}`

**Start stream**:
```bash
curl -X POST http://localhost:8080/api/v1/streams/<stream-id>/start
```

Expected: `{"status": "started"}`

**Verify packets** (in another terminal):
```bash
sudo tcpdump -i <interface> ether proto 0x88ba -v
```

Expected: SV packets with APP ID 0x4000

---

## ✅ Success Criteria

After completing these steps, you should have:

1. ✅ SV Publisher Manager managing streams
2. ✅ HTTP API creating/updating/starting streams
3. ✅ Phasor synthesis generating waveforms
4. ✅ SV packets transmitting on network
5. ✅ All code compiling without errors

---

## 📅 After This (Week 2+)

Once you have the above working:

1. **WebSocket Server** - Real-time data streaming
2. **COMTRADE Parser** - Playback mode
3. **GOOSE Subscriber** - Trip integration
4. **Analyzer Engine** - Receive and analyze external SV
5. **Sequence Engine** - Automated test sequences
6. **Test Modules** - Impedance, ramping, distance, etc.
7. **Frontend** - React UI for all modules

See `docs/IMPLEMENTATION_PROGRESS.md` for detailed roadmap.

---

## 🚨 Common Issues

### "Permission denied" when running

Raw sockets require root:
```bash
sudo ./build/Main
```

### "Cannot bind to port 8080"

Port already in use. Kill existing process:
```bash
lsof -ti:8080 | xargs kill -9
```

### Linker errors

Make sure CMakeLists.txt includes all libraries:
```cmake
target_link_libraries(Main PRIVATE vts_core vts_synth vts_api pthread)
```

### SV packets not visible

Check network interface, VLAN tagging, multicast routing.

---

## 🎓 Learning Resources

- **IEC 61850-9-2**: SV protocol specification
- **cpp-httplib docs**: https://github.com/yhirose/cpp-httplib
- **nlohmann-json docs**: https://json.nlohmann.me/
- **Raw sockets**: `man 7 raw`, `man 7 packet`

---

**Good luck! Start with the SV Publisher Manager and you'll have a working prototype in a day or two.**
