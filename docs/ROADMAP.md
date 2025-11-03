# Implementation Roadmap - Virtual TestSet

This document outlines the implementation strategy for the Virtual Test Set project based on the full integration implementation plan.

## Project Overview

The Virtual Test Set (VTS) is a comprehensive relay testing platform with:
- **Backend**: C++ high-performance vIED (virtual Intelligent Electronic Device)
- **Frontend**: React + TypeScript modern web UI
- **Infrastructure**: Docker-based deployment with RT capabilities

## Current Status

### ✅ Completed

1. **Docker Infrastructure** (docker/)
   - Dockerfile.backend - Multi-stage build with Ubuntu 24.04
   - Dockerfile.frontend - Node build + Nginx production
   - docker-compose.yml - Dev and RT profiles with health checks
   - nginx.conf - API/WS proxying configuration
   - README.md - Comprehensive deployment documentation

2. **JSON API Schemas** (schemas/)
   - stream-config.schema.json - SV stream configuration
   - phasor-update.schema.json - Phasor value updates
   - harmonics-update.schema.json - Harmonic components
   - sequence-run.schema.json - Multi-state test sequences
   - goose-config.schema.json - GOOSE subscription
   - ramp-test.schema.json - Ramping test configuration
   - impedance-injection.schema.json - Fault impedance
   - distance-test.schema.json - Distance relay testing
   - overcurrent-test.schema.json - Overcurrent relay testing
   - differential-test.schema.json - Differential relay testing

### 🚧 Remaining Implementation

The following sections detail what needs to be implemented to complete the project.

---

## Phase 1: Backend Core Infrastructure

### 1.1 Third-Party Dependencies

**Location**: `backend/third_party/`

Add as git submodules or vendored code:

```bash
cd backend
mkdir -p third_party

# HTTP Server
git submodule add https://github.com/yhirose/cpp-httplib third_party/cpp-httplib

# WebSocket
git submodule add https://github.com/zaphoyd/websocketpp third_party/websocketpp

# JSON
git submodule add https://github.com/nlohmann/json third_party/json

# ASIO (standalone)
git submodule add https://github.com/chriskohlhoff/asio third_party/asio
```

**CMake Integration**: Update `backend/CMakeLists.txt`:

```cmake
# Add third-party includes
include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/asio/asio/include
)

# Add API subdirectory
add_subdirectory(src/api)
```

### 1.2 HTTP & WebSocket API Server

**Files to Create**:
- `backend/src/api/CMakeLists.txt`
- `backend/src/api/include/http_server.hpp`
- `backend/src/api/src/http_server.cpp`
- `backend/src/api/include/ws_server.hpp`
- `backend/src/api/src/ws_server.cpp`
- `backend/src/api/include/json_validator.hpp`
- `backend/src/api/src/json_validator.cpp`

**Key Features**:
- REST endpoints for all module APIs (see API.md)
- WebSocket server for real-time streaming
- JSON schema validation against `schemas/`
- CORS handling for development
- Error handling and logging

**API Endpoints** (Full list in docs/API.md):
```
GET    /api/v1/health
GET    /api/v1/streams
POST   /api/v1/streams
PATCH  /api/v1/streams/{id}
DELETE /api/v1/streams/{id}
POST   /api/v1/streams/{id}/start
POST   /api/v1/streams/{id}/stop
POST   /api/v1/phasors/{streamId}
POST   /api/v1/phasors/{streamId}/harmonics
POST   /api/v1/comtrade/playback
POST   /api/v1/sequences/run
POST   /api/v1/sequences/stop
POST   /api/v1/goose/scan
POST   /api/v1/goose/config
POST   /api/v1/analyzer/select
POST   /api/v1/impedance/apply
POST   /api/v1/ramp/run
POST   /api/v1/distance/run
POST   /api/v1/overcurrent/run
POST   /api/v1/differential/run
```

### 1.3 SV Publisher Manager (Core)

**Files to Create**:
- `backend/src/core/CMakeLists.txt`
- `backend/src/core/include/sv_publisher_manager.hpp`
- `backend/src/core/src/sv_publisher_manager.cpp`
- `backend/src/core/include/sv_publisher_instance.hpp`
- `backend/src/core/src/sv_publisher_instance.cpp`

**Responsibilities**:
- Manage map of active SV streams (keyed by StreamID)
- CRUD operations on stream configurations
- Start/stop individual streams
- High-resolution tick function called from main loop
- Thread-safe access for API handlers

**Key Methods**:
```cpp
class SVPublisherManager {
public:
    std::string createStream(const StreamConfig& config);
    bool updateStream(const std::string& id, const StreamConfig& config);
    bool deleteStream(const std::string& id);
    std::vector<StreamInfo> listStreams() const;
    bool startStream(const std::string& id);
    bool stopStream(const std::string& id);
    bool updatePhasors(const std::string& id, const PhasorData& data);
    bool updateHarmonics(const std::string& id, const HarmonicsData& data);
    void tickAll(double timestamp); // Called from main loop
};
```

### 1.4 COMTRADE/CSV Parser

**Files to Create**:
- `backend/src/io/CMakeLists.txt`
- `backend/src/io/include/comtrade_parser.hpp`
- `backend/src/io/src/comtrade_parser.cpp`
- `backend/src/io/include/csv_parser.hpp`
- `backend/src/io/src/csv_parser.cpp`

**Features**:
- Parse `.cfg` files (channel names, sample rate, scaling factors a/b)
- Parse `.dat` files (ASCII and Binary formats)
- Parse `.csv` files as fallback
- Apply scaling: `value = a * raw + b`
- Sequential sample fetching for playback

**Test Files**:
- `backend/tests/unit/test_comtrade_parser.cpp`
- Include sample `.cfg/.dat` fixtures

### 1.5 Phasor Synthesizer with Harmonics

**Files to Create**:
- `backend/src/synth/CMakeLists.txt`
- `backend/src/synth/include/phasor_synth.hpp`
- `backend/src/synth/src/phasor_synth.cpp`

**Features**:
- Per-stream phasor state (8 channels: V-A, V-B, V-C, I-A, I-B, I-C, V-N, I-N)
- Fundamental frequency, magnitude, angle
- Per-channel harmonics array (up to 20 harmonics)
- Sample generation: `v[i] = Σ_n A_n*√2*sin(n*2πf*t_i + φ_n)`
- Deterministic timing using CLOCK_MONOTONIC

**Test Files**:
- `backend/tests/unit/test_phasor_synth.cpp`

### 1.6 GOOSE Subscriber with Trip Rule

**Files to Create**:
- Enhance existing `backend/src/sniffer/`
- `backend/src/sniffer/include/goose_subscriber.hpp`
- `backend/src/sniffer/src/goose_subscriber.cpp`
- `backend/src/sniffer/include/trip_rule_evaluator.hpp`
- `backend/src/sniffer/src/trip_rule_evaluator.cpp`

**Features**:
- libpcap sniffer for EtherType `0x88B8`
- Join multicast group
- ASN.1 parser with bounds checking
- Trip rule DSL evaluator (e.g., `RelayA_Trip/LLN0.Ind1.stVal == true`)
- Global atomic `TRIP_FLAG` accessible by sequence engine and testers
- WebSocket event emission on GOOSE messages

**Test Files**:
- `backend/tests/integration/test_goose_parser.cpp` (pcap replay)

### 1.7 Sequence Engine

**Files to Create**:
- `backend/src/sequence/CMakeLists.txt`
- `backend/src/sequence/include/sequence_engine.hpp`
- `backend/src/sequence/src/sequence_engine.cpp`

**Features**:
- Multi-stream state machine
- State with duration, phasors per stream, transition condition
- Transition types: time (automatic), gooseTrip (wait for TRIP_FLAG)
- Progress reporting via WebSocket
- Apply phasor states to SVPublisherManager

**Test Files**:
- `backend/tests/integration/test_sequence_engine.cpp`

### 1.8 Analyzer (Network Multimeter)

**Files to Create**:
- `backend/src/analyzer/CMakeLists.txt`
- `backend/src/analyzer/include/analyzer_engine.hpp`
- `backend/src/analyzer/src/analyzer_engine.cpp`

**Features**:
- Sniff selected external SV stream
- Ring buffer for sample collection
- 1-cycle sliding FFT (using kissfft or FFTW)
- Compute RMS, angle, frequency from FFT
- Extract harmonics from FFT bins
- WebSocket streaming: phasors (60 Hz), waveform chunks, harmonics

**Test Files**:
- `backend/tests/unit/test_analyzer_fft.cpp`

### 1.9 Algorithmic Testers

**Files to Create**:

**Impedance Injection (Module 6)**:
- `backend/src/testers/include/impedance_injector.hpp`
- `backend/src/testers/src/impedance_injector.cpp`
- Symmetrical components math
- Fault type enumeration (AG, BG, CG, AB, BC, CA, ABG, BCG, CAG, ABC)
- Convert R+jX to phasors using Thevenin equivalent

**Ramping Tester (Module 7)**:
- `backend/src/testers/include/ramp_tester.hpp`
- `backend/src/testers/src/ramp_tester.cpp`
- High-resolution timer loop
- Variable setter (magnitude or angle)
- TRIP_FLAG monitoring
- Pickup/dropoff/reset detection
- WebSocket progress updates

**Distance 21 Tester (Module 9)**:
- `backend/src/testers/include/distance_tester.hpp`
- `backend/src/testers/src/distance_tester.cpp`
- For each R-X point: sequence (pre-fault, fault)
- Measure trip time from fault entry
- WebSocket result streaming

**Overcurrent 50/51 Tester (Module 10)**:
- `backend/src/testers/include/overcurrent_tester.hpp`
- `backend/src/testers/src/overcurrent_tester.cpp`
- Inject DC=0, then current at multiples of pickup
- Time measurement
- Curve computation for frontend overlay

**Differential 87 Tester (Module 11)**:
- `backend/src/testers/include/differential_tester.hpp`
- `backend/src/testers/src/differential_tester.cpp`
- Given (Ir, Id), compute Is1 = Ir + Id/2, Is2 = -(Ir - Id/2)
- Apply to two streams
- Measure trip time

**Test Files**:
- `backend/tests/unit/test_symmetrical_components.cpp`
- `backend/tests/unit/test_ramp_logic.cpp`
- `backend/tests/unit/test_distance_calc.cpp`

---

## Phase 2: Frontend Application

### 2.1 Project Setup

**Install Dependencies**:

```bash
cd frontend
npm install -D tailwindcss postcss autoprefixer
npx tailwindcss init -p

npm install @radix-ui/react-dialog @radix-ui/react-dropdown-menu @radix-ui/react-select
npm install class-variance-authority clsx tailwind-merge lucide-react

npm install zustand
npm install @tanstack/react-router
npm install react-hook-form zod @hookform/resolvers
npm install recharts
npm install msw --save-dev
npm install @testing-library/react @testing-library/jest-dom vitest --save-dev
npm install -D @playwright/test
```

**Configure Tailwind** (`tailwind.config.ts`):

```typescript
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {},
  },
  plugins: [],
}
```

**Add shadcn/ui**:

```bash
npx shadcn-ui@latest init
npx shadcn-ui@latest add button card dialog input label select table tabs
```

### 2.2 App Shell and Routing

**Files to Create**:
- `frontend/src/App.tsx` - Main application component
- `frontend/src/layouts/AppLayout.tsx` - Shell with sidebar + topbar
- `frontend/src/components/Sidebar.tsx` - Navigation sidebar
- `frontend/src/components/Topbar.tsx` - Header with theme toggle

**Routes**:
- `/` - Dashboard
- `/streams` - SV Publisher Management (Module 13)
- `/manual` - Manual Phasor Injection (Module 2)
- `/comtrade` - COMTRADE/CSV Playback (Module 1)
- `/sequencer` - Sequence Test Builder (Module 3)
- `/analyzer` - Network Analyzer (Module 5)
- `/goose` - GOOSE Config (Module 4)
- `/tests/ramping` - Ramping Test (Module 7)
- `/tests/distance` - Distance 21 Test (Module 9)
- `/tests/overcurrent` - Overcurrent Test (Module 10)
- `/tests/differential` - Differential 87 Test (Module 11)
- `/impedance` - Impedance Injection (Module 6)
- `/settings` - Application Settings

### 2.3 State Management

**Zustand Stores** (`frontend/src/stores/`):

- `useStreamStore.ts` - SV streams state
- `useSequencerStore.ts` - Sequence builder state
- `useAnalyzerStore.ts` - Analyzer data
- `useGooseStore.ts` - GOOSE subscriptions and trip flag
- `useTestStore.ts` - Test execution state
- `useSettingsStore.ts` - App settings (theme, units)

### 2.4 API & WebSocket Clients

**Files to Create**:
- `frontend/src/lib/api.ts` - REST API client
- `frontend/src/lib/ws.ts` - WebSocket client with auto-reconnect
- `frontend/src/lib/types.ts` - TypeScript types matching schemas

**API Client Example**:

```typescript
class ApiClient {
  private baseUrl = '/api/v1';
  
  async getStreams() { /* ... */ }
  async createStream(config: StreamConfig) { /* ... */ }
  async updateStream(id: string, config: Partial<StreamConfig>) { /* ... */ }
  async deleteStream(id: string) { /* ... */ }
  async startStream(id: string) { /* ... */ }
  async stopStream(id: string) { /* ... */ }
  async updatePhasors(streamId: string, data: PhasorData) { /* ... */ }
  // ... all other endpoints
}
```

**WebSocket Client**:

```typescript
class WsClient {
  private ws: WebSocket | null = null;
  
  connect() { /* ... */ }
  disconnect() { /* ... */ }
  subscribe(topic: string, callback: (data: any) => void) { /* ... */ }
  unsubscribe(topic: string) { /* ... */ }
}
```

### 2.5 Module UI Components

**Module 13: SV Publisher Management**

**Files**:
- `frontend/src/pages/StreamsPage.tsx`
- `frontend/src/components/streams/StreamTable.tsx`
- `frontend/src/components/streams/StreamModal.tsx`
- `frontend/src/components/streams/StreamActions.tsx`

**Features**:
- Table with columns: Name, svID, APPID, MAC, VLAN, Rate, Status, Actions
- Add/Edit modal with form validation (React Hook Form + Zod)
- Start/Stop buttons
- Delete confirmation

**Module 2: Manual Phasor Injection**

**Files**:
- `frontend/src/pages/ManualInjectionPage.tsx`
- `frontend/src/components/manual/ActiveStreamsPanel.tsx`
- `frontend/src/components/manual/StreamCard.tsx`
- `frontend/src/components/manual/PhasorControls.tsx`
- `frontend/src/components/manual/HarmonicsEditor.tsx`

**Features**:
- Select active streams
- Per-stream card with Start/Stop
- Frequency slider
- 8 phasor controls (V-A, V-B, V-C, I-A, I-B, I-C, V-N, I-N)
- Magnitude slider + numeric input
- Angle slider + numeric input
- "Link 120°" button for balanced systems
- Harmonics panel (per channel)
- Debounced updates (100ms)

**Module 1: COMTRADE/CSV Playback**

**Files**:
- `frontend/src/pages/ComtradePage.tsx`
- `frontend/src/components/comtrade/FileUpload.tsx`
- `frontend/src/components/comtrade/ChannelMapTable.tsx`
- `frontend/src/components/comtrade/PlaybackControls.tsx`

**Features**:
- Drag-and-drop file upload (.cfg + .dat or .csv)
- Client-side .cfg parsing for preview
- Channel mapping table (file channels → SV channels)
- Loop checkbox
- Start/Stop/Pause controls
- Progress bar

**Module 3: Sequencer**

**Files**:
- `frontend/src/pages/SequencerPage.tsx`
- `frontend/src/components/sequencer/SequenceBuilder.tsx`
- `frontend/src/components/sequencer/StateCard.tsx`
- `frontend/src/components/sequencer/StateEditorModal.tsx`
- `frontend/src/components/sequencer/PhasorEditor.tsx`
- `frontend/src/components/sequencer/TransitionSelector.tsx`

**Features**:
- Drag-and-drop state reordering
- Add/Edit/Delete states
- Per-state modal:
  - Name, duration
  - Transition type (time/GOOSE)
  - Tabs for each active stream
  - Phasor editor per stream (same as Module 2)
- Start/Stop execution
- Live progress indicator
- Timeline visualization

**Module 4: GOOSE Config**

**Files**:
- `frontend/src/pages/GoosePage.tsx`
- `frontend/src/components/goose/GooseScanner.tsx`
- `frontend/src/components/goose/GooseTable.tsx`
- `frontend/src/components/goose/TripRuleEditor.tsx`

**Features**:
- Scan button → discovers GOOSE messages
- Table with: APPID, goCBRef, MAC, Last seen
- Manual subscription form
- Trip rule editor (textarea with syntax highlighting)
- Global trip flag indicator (badge in topbar)

**Module 5: Network Analyzer**

**Files**:
- `frontend/src/pages/AnalyzerPage.tsx`
- `frontend/src/components/analyzer/StreamSelector.tsx`
- `frontend/src/components/analyzer/WaveformChart.tsx`
- `frontend/src/components/analyzer/PhasorTable.tsx`
- `frontend/src/components/analyzer/VectorDiagram.tsx`
- `frontend/src/components/analyzer/HarmonicsChart.tsx`
- `frontend/src/components/analyzer/GooseMonitor.tsx`

**Features**:
- Start scan → list discovered SV streams
- Select stream to analyze
- Tabs:
  - Waveform: Oscilloscope (Recharts LineChart)
  - Phasors: Table + vector diagram (PolarAngleAxis)
  - Harmonics: Bar chart
  - GOOSE: Event log table

**Module 6: Impedance Injection**

**Files**:
- `frontend/src/pages/ImpedancePage.tsx`
- `frontend/src/components/impedance/ImpedanceForm.tsx`
- `frontend/src/components/impedance/SourceImpedancePanel.tsx`
- `frontend/src/components/impedance/FaultTypeSelector.tsx`

**Features**:
- Stream selector
- R and X inputs (ohms)
- Fault type dropdown
- Source impedance panel (RS1, XS1, RS0, XS0, Vprefault)
- Apply button → converts to phasors and updates stream

**Module 7: Ramping Test**

**Files**:
- `frontend/src/pages/RampingTestPage.tsx`
- `frontend/src/components/ramping/RampForm.tsx`
- `frontend/src/components/ramping/KpiCards.tsx`
- `frontend/src/components/ramping/RampChart.tsx`

**Features**:
- Stream selector
- Variable dropdown (I-A.mag, I-B.mag, etc.)
- Start/End/Step inputs
- Step duration slider
- Stop on trip checkbox
- Find dropoff checkbox
- Start button
- Live KPI cards: Pickup, Dropoff, Reset Ratio
- Optional: live chart showing ramp progress

**Module 9: Distance 21 Test**

**Files**:
- `frontend/src/pages/DistanceTestPage.tsx`
- `frontend/src/components/distance/RXCanvas.tsx`
- `frontend/src/components/distance/ZoneEditor.tsx`
- `frontend/src/components/distance/TestPointsTable.tsx`
- `frontend/src/components/distance/ResultsTable.tsx`

**Features**:
- Interactive R-X canvas (Recharts ScatterChart or custom SVG)
- Zone drawing tools (Mho, Quadrilateral)
- Test points table (Add/Edit/Delete points)
- Source impedance inputs
- Start test button
- Results table: Point, R, X, Fault Type, Trip Time, Pass/Fail
- Color-coded points on canvas

**Module 10: Overcurrent 50/51 Test**

**Files**:
- `frontend/src/pages/OvercurrentTestPage.tsx`
- `frontend/src/components/overcurrent/SettingsForm.tsx`
- `frontend/src/components/overcurrent/CurvePlot.tsx`
- `frontend/src/components/overcurrent/ResultsTable.tsx`

**Features**:
- Relay settings: Pickup, TMS, Curve type
- Test points generator (multiples of pickup)
- Start test button
- Log-log curve plot (Recharts) with:
  - Theoretical curve (computed client-side)
  - Actual measured points (from backend)
- Results table: Multiple, Expected Time, Actual Time, Error %

**Module 11: Differential 87 Test**

**Files**:
- `frontend/src/pages/DifferentialTestPage.tsx`
- `frontend/src/components/differential/SideSelector.tsx`
- `frontend/src/components/differential/SettingsForm.tsx`
- `frontend/src/components/differential/IdIrPlot.tsx`
- `frontend/src/components/differential/PointsTable.tsx`

**Features**:
- Side 1/2 stream selectors
- Relay settings: Slope K1/K2, Min pickup, Breakpoint
- Test points table (Ir, Id)
- Start test button
- Id vs Ir plot (Recharts ScatterChart) with:
  - Operating region (shaded)
  - Test points color-coded (pass/fail)
- Results table

### 2.6 Testing

**Vitest Unit Tests**:
- `frontend/src/components/__tests__/` - Component tests
- `frontend/src/stores/__tests__/` - Store tests
- `frontend/src/lib/__tests__/` - API client tests with MSW

**Playwright E2E Tests**:
- `frontend/tests/e2e/streams.spec.ts` - CRUD operations
- `frontend/tests/e2e/manual-injection.spec.ts` - Phasor updates
- `frontend/tests/e2e/analyzer.spec.ts` - Analyzer flow
- `frontend/tests/e2e/sequencer.spec.ts` - Sequence execution

---

## Phase 3: Integration & Testing

### 3.1 Backend Unit Tests

**Files to Create**:
- `backend/tests/unit/test_comtrade_parser.cpp`
- `backend/tests/unit/test_phasor_synth.cpp`
- `backend/tests/unit/test_symmetrical_components.cpp`
- `backend/tests/unit/test_ramp_logic.cpp`
- `backend/tests/unit/test_json_schemas.cpp`
- `backend/tests/unit/test_sv_publisher_manager.cpp`

### 3.2 Backend Integration Tests

**Files to Create**:
- `backend/tests/integration/test_http_api.cpp`
- `backend/tests/integration/test_ws_stream.cpp`
- `backend/tests/integration/test_sequence_engine.cpp`
- `backend/tests/integration/test_goose_parser.cpp` (pcap replay)

### 3.3 E2E Tests

**Scenarios**:
1. Create stream → Manual injection → Verify analyzer sees data
2. Upload COMTRADE → Map channels → Start playback
3. Build sequence → Execute → Verify state transitions
4. Configure GOOSE → Trigger trip → Verify flag
5. Run ramping test → Verify pickup/dropoff detection
6. Run distance test → Verify pass/fail results

---

## Phase 4: Documentation & CI/CD

### 4.1 Documentation

**Files to Create**:

**docs/API.md**:
- Complete REST API reference
- WebSocket message formats
- Authentication (if added)
- Rate limiting
- Error codes

**docs/DECISIONS.md**:
- Architectural Decision Records (ADR)
- Technology choices
- Design patterns
- Trade-offs

**docs/RUNBOOK.md**:
- Deployment procedures
- Monitoring
- Troubleshooting
- Performance tuning
- Security considerations

**Update READMEs**:
- `README.md` - Project overview
- `backend/README.md` - Backend specifics
- `frontend/README.md` - Frontend setup
- `docker/README.md` - Already created

### 4.2 CI/CD Pipeline

**File to Create**: `.github/workflows/ci.yml`

**Jobs**:
1. **Backend Build & Test**:
   - Build Docker image
   - Run unit tests (GoogleTest)
   - Run integration tests
   - Code coverage report
   - Static analysis (cppcheck, clang-tidy)

2. **Frontend Build & Test**:
   - Build Docker image
   - Run unit tests (Vitest)
   - Run linting (ESLint)
   - Type checking (TypeScript)
   - Build production bundle

3. **E2E Tests**:
   - Start backend + frontend with docker-compose
   - Run Playwright tests
   - Capture screenshots/videos on failure

4. **Security Scanning**:
   - Snyk scan for vulnerabilities
   - Container image scanning
   - Dependency audit

5. **Deploy** (if applicable):
   - Push images to registry
   - Deploy to staging/production

---

## Phase 5: Optimization & Hardening

### 5.1 Performance

- **Backend**:
  - Profile with perf/gprof
  - Optimize hot paths (sample generation, FFT)
  - Memory pool for allocations
  - Lock-free data structures where possible

- **Frontend**:
  - Code splitting
  - Lazy loading routes
  - Virtual scrolling for large tables
  - Debouncing/throttling
  - Web Workers for heavy computation

### 5.2 Real-Time Configuration

- CPU isolation
- IRQ affinity
- Thread pinning
- PREEMPT_RT kernel
- Huge pages
- See `backend/README-RT.md` for details

### 5.3 Security

- **Backend**:
  - Input validation (all endpoints)
  - Rate limiting
  - HTTPS/WSS in production
  - Authentication/Authorization (JWT)
  - Audit logging

- **Frontend**:
  - CSP headers
  - XSS protection
  - CSRF tokens
  - Secure cookies

- **Docker**:
  - Minimal base images
  - Non-root user
  - Read-only root filesystem
  - Capability dropping
  - Network segmentation

---

## Estimated Effort

| Phase | Estimated Time | Priority |
|-------|---------------|----------|
| Phase 1: Backend Core | 4-6 weeks | Critical |
| Phase 2: Frontend | 4-6 weeks | Critical |
| Phase 3: Integration & Testing | 2-3 weeks | High |
| Phase 4: Documentation & CI/CD | 1-2 weeks | High |
| Phase 5: Optimization | 2-3 weeks | Medium |

**Total**: 13-20 weeks for full implementation

---

## Success Criteria

- ✅ Docker compose up starts both services
- ✅ All REST endpoints respond correctly
- ✅ WebSocket streams deliver real-time data
- ✅ All 13 modules functional
- ✅ Unit test coverage ≥70%
- ✅ E2E tests pass
- ✅ Documentation complete
- ✅ CI/CD pipeline green
- ✅ Real-time performance validated

---

## Next Steps

1. **Immediate**: Set up third-party dependencies and HTTP/WS servers
2. **Week 1-2**: Implement SV Publisher Manager and Phasor Synthesizer
3. **Week 3-4**: Build REST API endpoints and basic frontend shell
4. **Week 5-6**: Implement COMTRADE parser and Manual Injection UI
5. **Week 7+**: Continue with remaining modules per priority

---

## References

- Implementation Plan: `/integration-imple.md`
- JSON Schemas: `/schemas/`
- Docker Config: `/docker/`
- IEC 61850-9-2: Sampled Values
- IEC 61850-8-1: GOOSE protocol
