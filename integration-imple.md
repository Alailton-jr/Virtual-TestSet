# Virtual Test Set (VTS) — Full Agent Build Plan (Frontend + Backend + Docker)

> **You are an implementation agent.** Follow the steps below to implement the React frontend and the C++ vIED backend enhancements for the Virtual Relay Test Set. Deliver clean, modern UI and deterministic, well‑tested backend. Both services must run via Docker and start together with health checks. When a detail is ambiguous, choose safe defaults and document them in `docs/DECISIONS.md`.

---

## 0) Conventions & Goals

* **Repo layout (monorepo):**

  * `frontend/` (React + TS + Vite)
  * `backend/` (C++ vIED, CMake, GoogleTest)
  * `docker/` (Dockerfiles, compose, runtime scripts)
  * `schemas/` (JSON Schemas for API contracts)
  * `tests/e2e/` (Playwright/CLI flows)
  * `docs/` (ADR, API, Runbooks)
* **UI style:** TailwindCSS + shadcn/ui (Radix), Recharts for plots, React Hook Form + Zod, Zustand for state.
* **Transport:** REST (control/data push triggers), WebSocket (telemetry / analyzer streaming), file upload via `multipart/form-data`.
* **Time base:** Use backend CLOCK_MONOTONIC. Frontend renders server timestamps.
* **Coding standards:**

  * C++17, `-Wall -Wextra -Wpedantic -Werror`, clang-format (Google).
  * React TS strict mode; ESLint + Prettier.

---

## 1) High-Level Task Breakdown (you may parallelize)

### Infra & DevX

1. **Docker & Compose**

   * Create `docker/docker-compose.yml` with services `backend`, `frontend`, shared network `vts_net`.
   * Add RT-friendly capabilities to `backend` container: `cap_add: ["NET_ADMIN","NET_RAW","SYS_NICE"]`, `ulimits` for rtprio/memlock.
   * Provide two profiles: `dev` (bridge network) and `rt` (host network) toggled via `COMPOSE_PROFILES`.
   * Healthchecks for both services.
2. **Reverse proxy (optional)**: For prod, serve `frontend` via Nginx; proxy `/api` and `/ws` to backend.
3. **CI**: GitHub Actions building both images, running unit tests (GTest, Vitest) and e2e (Playwright headless).

### Backend (C++ vIED)

4. **HTTP & WS gateway**

   * Vendor-in single-header libs: `cpp-httplib` (REST), `websocketpp` + ASIO (WS). Add as submodules under `backend/third_party/`.
   * Implement server in `src/api/http_server.cpp` and `src/api/ws_server.cpp`.
5. **SV Publisher Manager (Core)**

   * Implement `SVPublisherManager` class managing `SVPublisherInstance` map keyed by `StreamID`.
   * Each instance holds network config, active flags, data source (Synthesizer/COMTRADE), phasor & harmonic state.
   * Integrate with main high-resolution loop to iterate active instances and transmit SV.
6. **COMTRADE/CSV Player**

   * Parser: `.cfg` (channels, fs, scaling a/b), `.dat` ASCII/Binary; `.csv` fallback.
   * Data source: sequential sample fetch + scaling `value = a*raw + b`; timing derived from sample rate.
7. **Phasor Synthesizer & Harmonics**

   * Phasor state per stream (8 channels, Mag/Angle/Freq); fundamental + per-channel harmonics array.
   * Sample equation: `v[i] = Σ_n A_n*√2*sin(n*2πf*t_i + φ_n)`.
8. **GOOSE Subscriber**

   * `libpcap` sniffer for EtherType `0x88B8`; ASN.1 parser; join multicast; evaluate user-defined trip rule; set global `TRIP_FLAG`.
9. **Sequence Engine**

   * Accept multi-stream state array with durations & transition conditions (time or GOOSE trip).
   * Apply per-state phasors to streams; manage timers; report progress via WS.
10. **Analyzer (Network Multimeter)**

* Sniff SV/GOOSE; for selected external SV stream: buffer samples; 1-cycle FFT to compute Mag/Angle/Freq; harmonics from FFT bins; push via WS.

11. **Ramping/Distance/Overcurrent/Differential testers**

* Implement algorithmic kernels and small sequencers as per module specs (details below).

12. **REST API surface & JSON Schemas**

* CRUD for streams, phasor update, harmonics update, COMTRADE playback attach, sequence submit, testers start/stop, GOOSE config, analyzer select stream.
* Place schemas in `schemas/` and validate incoming JSON.

13. **Unit & integration tests**

* GoogleTest suites for parsers, engines, JSON validation, timing deltas; pcap replay tests for sniffer.

### Frontend (React + TS + Vite)

14. **Design system & Layout**

* Install Tailwind + shadcn/ui; app shell with left sidebar + topbar; light/dark.

15. **State, Routing & API client**

* Zustand stores; TanStack Router or React Router; `apiClient` with fetch wrappers; WS client with reconnect.

16. **Module screens & components**

* Implement pages and widgets per module (detailed below), including file upload + mapping UI, phasor sliders, sequence builder, analyzer views, R‑X editor, charts.

17. **Validation & forms**

* React Hook Form + Zod for all forms (stream configs, testers, sequence states).

18. **Tests**

* Vitest + RTL component tests; MSW to mock REST/WS; Playwright basic e2e flows.

---

## 2) API Contracts (REST + WS)

Create `schemas/*.json` and keep the backend validator in sync.

### 2.1 Streams (Module 13)

* `GET /api/v1/streams` → `{ streams: Stream[] }`
* `POST /api/v1/streams` → create from `StreamConfig`
* `PATCH /api/v1/streams/{id}` → partial update
* `DELETE /api/v1/streams/{id}`
* `POST /api/v1/streams/{id}/start` / `{id}/stop`

**StreamConfig**

```json
{
  "name": "Main Feeder MU",
  "svID": "SV1",
  "appIdHex": "0x4000",
  "macDst": "01:0C:CD:04:00:02",
  "vlanId": 100,
  "vlanPriority": 4,
  "datSet": "MU1/LLN0$DS1",
  "confRev": 1,
  "smpRate": 4800,
  "noChannels": 8
}
```

### 2.2 Phasors & Harmonics (Modules 2 & 8)

* `POST /api/v1/phasors/{streamId}` body: `{ freq, channels: { "V-A": { mag, angleDeg }, ... } }`
* `POST /api/v1/phasors/{streamId}/harmonics` body: `{ channel: "I-A", harmonics: [{ n: 3, mag, angleDeg }, ...] }`

### 2.3 COMTRADE/CSV Playback (Module 1)

* `POST /api/v1/comtrade/playback` (multipart)

  * fields: `streamId`, `loop` (bool)
  * files: `cfg`, `dat` **or** `csv`
  * returns parsed metadata & channel list

### 2.4 Sequencer (Module 3)

* `POST /api/v1/sequences/run`

```json
{
  "activeStreams": ["SV1","SV2"],
  "states": [
    {
      "name": "Pre-Fault",
      "durationSec": 2.0,
      "transition": { "type": "time" },
      "phasors": { "SV1": { "freq": 60, "channels": {"V-A": {"mag": 69e3/√3, "angleDeg": 0}, ... } } }
    },
    {
      "name": "Fault",
      "durationSec": 1.0,
      "transition": { "type": "gooseTrip" },
      "phasors": { "SV1": { ...fault state... }, "SV2": { ... } }
    }
  ]
}
```

* `POST /api/v1/sequences/stop`

### 2.5 GOOSE Subscriber (Module 4)

* `POST /api/v1/goose/scan` → `{ entries: ... }`
* `POST /api/v1/goose/config` → subscription params and **trip rule** expression (see below)
* **Trip rule** DSL (simple): e.g., `RelayA_Trip/LLN0.Ind1.stVal == true`

### 2.6 Analyzer (Module 5)

* `POST /api/v1/analyzer/select` body: `{ svStreamMac: "01:0C:CD:04:00:02" }`
* WS: `/ws/analyzer` → sends frames:

```json
{
  "type": "phasors",
  "ts": 1728000001.123456,
  "data": [ {"channel": "V-A", "mag": 39960, "angleDeg": -1.2, "freq": 60.01}, ...],
  "harmonics": [{"n":2, "mag": ...}]
}
```

Other message types: `waveform`, `gooseEvent`, `sequenceProgress`.

### 2.7 Impedance Injection (Module 6)

* `POST /api/v1/impedance/apply` body: `{ streamId, faultType, R, X, source: { RS1, XS1, RS0, XS0, Vprefault } }`

### 2.8 Ramping (Module 7)

* `POST /api/v1/ramp/run` body:

```json
{ "streamId": "SV1", "variable": "I-A.mag", "start": 0, "end": 10, "step": 0.1, "stepDurationMs": 50, "stopOnTrip": true, "findDropoff": true }
```

* Returns `{ pickup, dropoff, resetRatio }` when finished (also emits WS updates).

### 2.9 Distance 21 (Module 9)

* `POST /api/v1/distance/run` body: `{ streamId, source, points: [{R,X,faultType}, ...] }` → returns per‑point trip time; also emits WS.

### 2.10 Overcurrent 50/51 (Module 10)

* `POST /api/v1/overcurrent/run` body: `{ streamId, pickup, tms, curve, points: [1.1,1.5,2,3,5] }`.

### 2.11 Differential 87 (Module 11)

* `POST /api/v1/differential/run` body: `{ side1: "SV1", side2: "SV2", points: [{Ir,Id}], settings: {...} }`.

---

## 3) Backend Implementation Steps (C++)

1. **Project wiring**

   * Create `backend/third_party/` and add submodules: `cpp-httplib`, `websocketpp`, `json` (nlohmann), `asio`.
   * Extend `CMakeLists.txt` to build `api` component and link to core.

2. **SV Publisher Manager**

   * Files: `src/core/sv_publisher_manager.{hpp,cpp}`, `src/core/sv_publisher_instance.{hpp,cpp}`.
   * Methods: `create/update/destroy/list/start/stop`, `tickAll(now)`.

3. **COMTRADE/CSV Parser**

   * Files: `src/io/comtrade.{hpp,cpp}`; support ASCII/Binary; scaling a,b.
   * Unit tests with sample `.cfg/.dat` fixtures.

4. **Phasor & Harmonic Synthesizer**

   * Files: `src/synth/phasor_synth.{hpp,cpp}`; state per stream; sample generation.
   * Add harmonic arrays per channel.

5. **GOOSE Subscriber**

   * Files: `src/sniffer/goose_sniffer.{hpp,cpp}`; libpcap filter `ether proto 0x88b8`.
   * ASN.1 parser with bounds checks; global `TripFlag` guarded by atomic.

6. **Sequence Engine**

   * Files: `src/sequence/sequence_engine.{hpp,cpp}` with `State` model.
   * Drives `SVPublisherManager` phasor states.

7. **Analyzer DSP**

   * Files: `src/analyzer/analyzer_engine.{hpp,cpp}`; ring buffer; FFT (kissfft or custom); 1-cycle sliding FFT.
   * WS push encoder.

8. **Algorithmic testers**

   * **Impedance→Phasor (Module 6):** symmetrical components formulation → eight (V/I) phasors.
   * **Ramping (Module 7):** high-res timer loop with TRIP_FLAG latch.
   * **Distance 21 (Module 9):** state sequence pre‑fault/fault + timer; report ms.
   * **Overcurrent 50/51 (Module 10):** inject DC = 0, then currents at multiples; compute theory in frontend for overlay.
   * **Differential 87 (Module 11):** given (Ir,Id) compute `Is1`, `Is2` and sequence.

9. **REST & WS server**

   * Endpoints per Section 2; validate against `schemas/` using `nlohmann::json` + manual checks.
   * WS topics: analyzer data, goose events, sequence progress.

10. **Tests (GoogleTest)**

* `tests/unit/test_comtrade.cpp`, `test_phasor.cpp`, `test_symcomp.cpp`, `test_ramp.cpp`, `test_json_schemas.cpp`.
* Integration: `tests/integration/test_ws_stream.cpp`, pcap replay for GOOSE parse, sequence end‑to‑end with stub TX.

11. **Build & Run**

* Update `backend/README.md` with new CLI: `--api-port`, `--ws-port`.

---

## 4) Frontend Implementation Steps (React + TS)

1. **Install base UI**

   * Tailwind + shadcn/ui; configure theme tokens; global container with responsive grid.

2. **App shell & routes**

   * Sidebar: Dashboard, SV Publishers, Manual Injection, Sequencer, Analyzer, Tests (Ramping/Distance/OC/Diff), GOOSE Config, Settings.

3. **State & clients**

   * `lib/api.ts` (REST wrappers), `lib/ws.ts` (WS with auto-reconnect); `stores/*.ts` (Zustand slices: streams, sequencer, analyzer).

4. **Module UIs**

   * **SV Publisher Management (Module 13)**: table + add/edit modal with validation.
   * **COMTRADE/CSV Playback (Module 1)**: upload (.cfg/.dat/.csv), parse client-side metadata; mapping UI (file channels → SV channels) via DnD table; progress bar; Start/Stop.
   * **Manual Phasor Injection (Module 2)**: Active Streams panel; per‑stream card with Start/Stop, Freq, 8 phasor controls (sliders + numeric), "Link 120°"; live send on change.
   * **Sequencer (Module 3)**: Builder with re-orderable states; per-state Phasor Editor (tabs per stream); Duration; Transition (time/GOOSE trip); Start/Stop.
   * **GOOSE Config (Module 4)**: network scan table; manual form; trip-rule editor (mini DSL + examples).
   * **Analyzer (Module 5)**: Start/Stop scan; discovered SV list; Waveform view (oscilloscope plot), Phasor table + vector diagram + bar chart for harmonics; GOOSE monitor table.
   * **Impedance Toggle (Module 6)**: Shared UI in Modules 2/3 to switch to R+jX inputs, source impedances panel.
   * **Ramping (Module 7)**: target stream, variable dropdown, start/end/step/duration; checkboxes; live KPIs (Pickup/Dropoff/Reset).
   * **Distance 21 (Module 9)**: large R‑X canvas with zone editor (Mho/Quad), test points, results table; overlay pass/fail.
   * **Overcurrent 50/51 (Module 10)**: relay settings; test points generator; log‑log theoretical curve (drawn in React) + incoming actual points.
   * **Differential 87 (Module 11)**: Side 1/2 selectors; settings; plot Id vs Ir; points pass/fail.

5. **Charts**

   * Recharts components: LineChart (waveform), PolarAngleAxis vector diagram, BarChart (harmonics), ScatterChart (R‑X, Id/Ir).

6. **Testing (frontend)**

   * Unit: component behavior (sliders send debounced updates), form validation.
   * Integration: pages mocking REST/WS via MSW.
   * E2E: Playwright basic paths (create stream → manual injection → see analyzer data → run sequencer).

7. **UX polish**

   * Debounce phasor updates (e.g., 60–100 ms) to avoid flooding backend.
   * Toasters for success/fail; skeleton loaders; empty states; keyboard accessibility.

---

## 5) Docker & Compose

**backend Dockerfile** (`docker/Dockerfile.backend`)

```dockerfile
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y build-essential cmake libpcap-dev git && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY backend/ ./backend/
WORKDIR /app/backend
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j$(nproc)

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y libpcap0.8 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/backend/build/vts /usr/local/bin/vts
COPY backend/schemas /app/schemas
ENV IF_NAME=eth0 API_PORT=8080 WS_PORT=8090
HEALTHCHECK --interval=10s --timeout=2s --retries=5 CMD nc -z localhost $API_PORT || exit 1
CMD ["/usr/local/bin/vts","--api-port","8080","--ws-port","8090"]
```

**frontend Dockerfile** (`docker/Dockerfile.frontend`)

```dockerfile
FROM node:20-alpine AS build
WORKDIR /app
COPY frontend/package*.json ./
RUN npm ci
COPY frontend/ ./
RUN npm run build

FROM nginx:alpine
COPY docker/nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
HEALTHCHECK CMD wget -qO- http://localhost/ >/dev/null || exit 1
```

**nginx.conf** (`docker/nginx.conf`)

```nginx
server {
  listen 80;
  location / { try_files $uri /index.html; }
  location /api { proxy_pass http://backend:8080; }
  location /ws { proxy_pass http://backend:8090; proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade; proxy_set_header Connection "Upgrade"; }
}
```

**docker-compose.yml** (`docker/docker-compose.yml`)

```yaml
version: "3.9"
name: vts

networks:
  vts_net:
    driver: bridge

services:
  backend:
    build:
      context: ..
      dockerfile: docker/Dockerfile.backend
    image: vts-backend:latest
    networks: [vts_net]
    cap_add: ["NET_ADMIN","NET_RAW","SYS_NICE"]
    ulimits:
      rtprio: 95
      memlock: -1
    environment:
      - IF_NAME=${IF_NAME:-eth0}
      - API_PORT=8080
      - WS_PORT=8090
    healthcheck:
      test: ["CMD","nc","-z","localhost","8080"]
      interval: 10s
      timeout: 2s
      retries: 5

  frontend:
    build:
      context: ..
      dockerfile: docker/Dockerfile.frontend
    image: vts-frontend:latest
    depends_on:
      backend:
        condition: service_healthy
    networks: [vts_net]
    ports:
      - "5173:80"

# profile for host network real-time
x-profiles:
  rt:
    services:
      backend:
        network_mode: host
        ports: []
```

---

## 6) Detailed UI Specs per Module

### Module 1 – COMTRADE/CSV Playback

* **Components**: `ComtradeUpload`, `ChannelMapTable`, `PlaybackProgress`.
* **Flow**: upload → parse metadata client-side for preview → send file(s) + mapping via REST → show backend progress via WS `sequenceProgress` or `playbackProgress`.
* **Tests**: parsing `.cfg` fixtures; mapping integrity; API call with `FormData`.

### Module 2 – Manual Phasor Injection

* **Components**: `ActiveStreamsPanel`, `StreamCard`, `PhasorGroup` (8 groups), `AngleLink120`.
* **Flow**: add stream → Start → slider/input changes send debounced `POST /phasors/{id}`.
* **Tests**: UI debounces; store updates; API calls.

### Module 3 – Sequence Test

* **Components**: `SequenceBuilder` (DnD list), `StateEditorModal` (tabs per stream), `TransitionPicker`.
* **Flow**: Build states → Start → live progress; Stop.
* **Tests**: Serialization correctness; duration sum; trip transition handling (mock WS).

### Module 4 – GOOSE Subscriber Config

* **Components**: `GooseScanner`, `TripRuleEditor` (syntax highlighting), `GooseTable`.
* **Flow**: Scan → select/confirm → save config → trip flag indicator (global badge in header).
* **Tests**: rule validation; scan table rendering; config submit.

### Module 5 – Network Multimeter / Analyzer

* **Components**: `WaveformChart`, `PhasorTable`, `VectorDiagram`, `HarmonicsBar`, `GooseMonitor`.
* **Flow**: Start scan → pick SV stream → charts update via WS (60 Hz phasor frames; waveform chunks).
* **Tests**: chart rendering with mocked WS; pause/resume.

### Module 6 – Impedance Injection

* **Components**: `ImpedancePanel` (R,X, fault type), `SourceImpedancePanel` (shared with settings), toggle in Modules 2/3.
* **Flow**: when Impedance mode, compute phasors on backend and update stream state.
* **Tests**: form validation; backend call made.

### Module 7 – Ramping Test

* **Components**: `RampForm`, `KpiCards` (Pickup/Dropoff/Reset), `RampLiveChart` (optional).
* **Tests**: correct payload; WS updates drive KPIs.

### Module 9 – Distance (21) Tester

* **Components**: `RXCanvas` (draw zones: Mho/Quad), `TestPointsTable`.
* **Tests**: zone editing persistence; point colorization on results.

### Module 10 – Overcurrent (50/51)

* **Components**: `IDMTSettings`, `CurvePlot`, `ResultsTable`.
* **Tests**: theoretical curve drawn from settings; incoming results plotted.

### Module 11 – Differential (87)

* **Components**: `IdIrPlot`, `PointsEditor`, `SettingsForm`, Side selectors.
* **Tests**: compute derived side currents client-side for preview; results overlay.

### Module 13 – SV Publisher Management

* **Components**: `StreamTable`, `StreamModal` (add/edit), actions start/stop.
* **Tests**: CRUD happy paths; validations (hex APPID, MAC format, VLAN ranges).

---

## 7) Backend Algorithm Notes & Unit Tests

1. **Symmetrical Components (Module 6)**

   * Implement α operator and sequence networks; compute per‑fault type phasors.
   * **Tests**: compare against analytic cases; L‑G, L‑L, L‑L‑G, 3‑φ.

2. **Ramping (Module 7)**

   * High-resolution loop; latch pickup/dropoff upon `TRIP_FLAG` transitions.
   * **Tests**: simulate GOOSE flag toggles; verify thresholds.

3. **Distance (Module 9)**

   * For each (R,X) → compute V/I via Thevenin equivalent using provided source impedances.
   * **Tests**: fixed source/Zfault pairs with known trips (mock trip via injected logic if needed).

4. **Overcurrent (Module 10)**

   * Inject currents at multiples; timer start on Fault state entry.
   * **Tests**: verify time measurement monotonic & within tolerance with mocked clock.

5. **Differential (Module 11)**

   * Given (Ir,Id), compute side currents: `Is1 = Ir + Id/2`, `Is2 = -(Ir - Id/2)`.
   * **Tests**: identity checks for Ir/Id definitions.

6. **Analyzer FFT (Module 5)**

   * Use Hann window; 1‑cycle FFT around 60 Hz; report Mag (RMS), Angle (deg), Freq.
   * **Tests**: synthetic pure sine yields correct magnitude & angle within tolerance.

7. **COMTRADE Parser (Module 1)**

   * **Tests**: `.cfg` parsing robust to whitespace, comments; ASCII/Binary `.dat` read with scaling.

---

## 8) Test Matrix

| Layer    | Suite         | What it verifies                                             |
| -------- | ------------- | ------------------------------------------------------------ |
| Backend  | Unit: parsers | COMTRADE/CSV, JSON schemas                                   |
| Backend  | Unit: math    | SymComp, harmonics, ramping timers                           |
| Backend  | Unit: manager | SVPublisherManager lifecycle                                 |
| Backend  | Integration   | REST/WS endpoints; sequence engine flows                     |
| Backend  | Pcap          | GOOSE parse & filter                                         |
| Frontend | Unit          | Components: forms, sliders (debounce), tables                |
| Frontend | Integration   | Pages with MSW mocks (CRUD, sequences)                       |
| E2E      | Playwright    | Create stream → manual inject → analyzer → run sequence/test |

---

## 9) Developer Commands

```bash
# Build & run (compose)
cd docker && docker compose up --build

# Run backend unit tests in container
docker compose exec backend /usr/local/bin/vts --selftest || true

# Frontend tests
docker compose exec frontend sh -lc "npm test"

# E2E tests (headless)
cd tests/e2e && npx playwright test
```

---

## 10) Acceptance Criteria (per module)

* **Module 1**: Upload COMTRADE; map channels; Start; progress visible; SV packets emitted at file sample rate.
* **Module 2**: Adjust sliders; backend phasors update immediately; multiple streams simultaneously.
* **Module 3**: Multi‑stream states apply correctly; time/GOOSE transitions work; progress events visible.
* **Module 4**: Scan shows GOOSE entries; manual config saved; trip flag toggles with received stVal; global badge updates.
* **Module 5**: Selected SV stream shows waveform + phasors + harmonics; GOOSE monitor updates in real time.
* **Module 6**: R+jX inputs generate correct phasors (unit tests); applied to stream.
* **Module 7**: Pickup/dropoff/reset computed; results rendered.
* **Module 9**: R‑X plot shows pass/fail with measured trip times.
* **Module 10**: Points plotted on IDMT curve with measured times.
* **Module 11**: Id/Ir results color‑coded; Side1/Side2 currents consistent with definitions.
* **Module 13**: CRUD of streams; Start/Stop toggles state; persisted in backend memory (optional JSON file persistence).

---

## 11) Files to Create/Modify (Checklist)

**Backend**

* `src/api/http_server.cpp`, `include/api/http_server.hpp`
* `src/api/ws_server.cpp`, `include/api/ws_server.hpp`
* `src/core/sv_publisher_manager.*`, `src/core/sv_publisher_instance.*`
* `src/io/comtrade.*`
* `src/synth/phasor_synth.*`
* `src/sniffer/goose_sniffer.*`
* `src/analyzer/analyzer_engine.*`
* `src/sequence/sequence_engine.*`
* `tests/unit/*.cpp`, `tests/integration/*.cpp`
* `schemas/*.json`
* `CMakeLists.txt`

**Frontend**

* `src/app/AppShell.tsx`, `src/routes/*`
* `src/components/*` (tables, charts, editors)
* `src/stores/*`
* `src/lib/api.ts`, `src/lib/ws.ts`
* `src/styles/tailwind.css`, `tailwind.config.ts`
* `tests/*` (Vitest) and `playwright.config.ts`

**Infra**

* `docker/Dockerfile.backend`, `docker/Dockerfile.frontend`
* `docker/docker-compose.yml`, `docker/nginx.conf`
* `docs/API.md`, `docs/DECISIONS.md`, `docs/RUNBOOK.md`

---

## 12) Implementation Order Recommendation (fastest value first)

1. Infra (compose + healthchecks) → visible environment.
2. Backend HTTP/WS skeleton + `/streams` CRUD → Frontend can start integrating.
3. Frontend shell + SV Publisher Management screen.
4. Phasor Synth + Manual Injection end‑to‑end.
5. Analyzer WS stream (waveform + phasors) for feedback loop.
6. COMTRADE playback.
7. Sequence engine.
8. GOOSE subscriber + trip rule.
9. Ramping / Impedance / Distance / Overcurrent / Differential.
10. Hardening tests + e2e + docs.

---

## 13) Notes & Pitfalls

* Rate‑limit phasor updates from UI; coalesce by stream.
* Validate APPID (hex) and MAC format strictly.
* When on macOS dev, allow `--no-net` mode; expose simulated WS data.
* In Docker RT profile, consider `network_mode: host` for lowest latency and access to raw sockets; document security implications.
* For file uploads, store temp files under `/app/data/uploads` (container volume) and clean after run.

---

## 14) Deliverables

* Running `docker compose up` shows frontend on `http://localhost:5173/` and a healthy backend.
* All APIs per Section 2 respond; WS streams deliver analyzer data.
* Unit tests ≥ 70% backend lines on new modules; frontend Vitest for core components.
* Playwright e2e covering at least: stream CRUD, manual inject, analyzer, sequence start/stop.
* Updated READMEs and runbooks.
