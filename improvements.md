
# Prompt: Virtual Test Set — Fixes, Real-Time Docker (Linux RT), and macOS Portability

**Role:** You are a senior C++/real-time/networking engineer. Apply the phases below to this repository (Virtual Test Set only). Do each phase as an individual, focused set of commits. If any required file or symbol is missing, write a `PARTIAL` log line and stop at that step until clarified.

> **Mandatory progress log** (after every numbered step and major sub-step):
> Append a single line to `AGENT_PROGRESS_VTS.txt`:
>
> ```
> [YYYY-MM-DD HH:MM:SS] STEP <id> DONE — <one-line summary> (files: ..., tests: ..., notes: ...)
> ```
>
> Use `PARTIAL` if blocked and state the blocker.

---

## Phase 0 — Preflight (no behavior changes)

0.1 **Inventory** TX/RX and platform-specific code

* Grep for: `AF_PACKET`, `TPACKET`, `PACKET_`, `SO_TIMESTAMPING`, `sendto`, `sendmsg`, `RawSocket`, `SCHED_FIFO`, `mlockall`, `clock_nanosleep`.
* Note where GOOSE/SV are encoded/decoded and where timers, threads, and “digital inputs” are handled.

0.2 **Tight toolchain**

* In root `CMakeLists.txt`: enable `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Werror`.
* Add options `ENABLE_ASAN`, `ENABLE_TSAN`, `ENABLE_UBSAN` that append the usual sanitizer flags.
* Add a basic `ctest` target (even a placeholder test that runs the binary with `--help`).
* **Commit:** `chore(build): strict warnings, sanitizer presets, and minimal ctest`
* **Log:** `0-preflight`

---

## Phase 1 — P0 Hotfixes from the Audit (must pass before anything else)

1.1 **Concurrency: digital_input**

* Replace `std::vector<uint8_t> digital_input` with `std::vector<std::atomic<uint8_t>>`.
* All writers: `store(val, memory_order_release)`; readers: `load(memory_order_acquire)`.
* Remove global duplicates; carry the reference via constructor/plan structs.
* **Commit:** `fix(concurrency): atomic digital_input with acquire/release semantics`
* **Acceptance:** TSAN clean under concurrent sniffer + tests.

1.2 **ASN.1 length bounds (GOOSE/SV decoders)**

* Before every buffer access: assert `offset < frameSize`.
* Implement multi-byte BER lengths: short (≤0x7F), 0x81, 0x82 cases; reject bigger values.
* **Commit:** `fix(parser): full BER length handling and bounds checks`
* **Acceptance:** Fuzz 1M malformed packets → no crash.

1.3 **Timers use CLOCK_MONOTONIC**

* In all timing helpers: `clock_gettime(CLOCK_MONOTONIC, ...)`.
* `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)`, retry on `EINTR`.
* **Commit:** `fix(rt): monotonic clocks and EINTR-safe sleeps`
* **Acceptance:** NTP jump does not skew inter-packet cadence.

1.4 **smpCnt wrap**

* Mask to 16 bits on encode/update (`static_cast<uint16_t>(smpCnt+1)`).
* Where rate-based modulo is correct, use it (e.g., `smpCnt = (smpCnt+1) % smpRate`) without corrupting adjacent fields.
* **Commit:** `fix(protocol): 16-bit smpCnt wrap`
* **Acceptance:** 70,000 samples run; correct wrapping.

1.5 **save_file path sanitization + sizeof bug**

* Reject `..`, `/`, non-printables; build path only under a whitelisted directory (e.g., `files/`), using `std::filesystem`.
* Replace `sizeof(buffer)` on pointers with explicit max buffer size (`std::array` or length parameter).
* Replace `atoi` with `std::stoi` + bounds and error checks.
* **Commit:** `fix(security): sanitized file paths and robust size parsing`
* **Acceptance:** Path traversal rejected; large file transfer completes.

1.6 **RawSocket robustness**

* Replace `exit(1)` with `throw std::runtime_error(...)`.
* Check `if_nametoindex` (0 → throw).
* Critical setsockopt failures (e.g., QDISC_BYPASS): throw; optional (timestamping): WARN.
* **Commit:** `fix(net): exception-based RawSocket errors and if_nametoindex checks`
* **Acceptance:** Non-root: clean error; invalid iface: helpful message.

1.7 **GOOSE allData length (>255)**

* Use BER long-form lengths for >0x7F/0xFF.
* **Commit:** `fix(goose): long-form BER length for allData`
* **Acceptance:** Encode 300 booleans; parse correctly.

1.8 **Virtual_LAN validation**

* Validate priority 0..7 and VLAN ID ≤ 4095 at construction.
* **Commit:** `fix(vlan): parameter validation and private members`
* **Acceptance:** priority=8 rejected.

1.9 **Sniffer globals & stoppability**

* Remove global sniffer/digital_input pointers. Pass instance via task args.

* Add `SO_RCVTIMEO` (e.g., 100ms). Stop flag is atomic; check it after timeout.

* **Commit:** `fix(sniffer): instance-local state and responsive stop`

* **Acceptance:** Stop with no traffic exits within 200ms.

* **Log:** `1-hotfixes`

---

## Phase 2 — Protocol Layer Canonicalization & Performance

2.1 **Single source of truth**

* Resolve duplication between `Protocols.hpp` and standalone headers—keep only one canonical set (prefer modular standalone headers).
* **Commit:** `refactor(protocol): remove duplicates; single canonical encoders`

2.2 **BER encoding (constructed & primitive)**

* Definite-length BER for every constructed tag; compute lengths recursively; guard dataset counts.
* **Commit:** `fix(protocol): recursive BER lengths and dataset bounds`

2.3 **MAC parsing & VLAN**

* Fix off-by-one in MAC parser; pre-allocate or return fixed arrays where possible.

* Keep VLAN fields private; add setters with validation if needed.

* **Commit:** `fix(eth): MAC parsing guardrails and preallocation`

* **Acceptance:** Golden packets byte-exact; no heap churn in hot loop.

* **Log:** `2-protocol-canonical`

---

## Phase 3 — Threading discipline & ThreadPool

3.1 **Stop/join semantics**

* No `detach()`. Every module exposes `start()`, `stop()`, `join()`.
* All `pthread_cond_wait` inside `while (!predicate)` loops; broadcasts with mutex held.

3.2 **ThreadPool**

* Initialize members before starting workers; check `pthread_create` return.

* Destructor drains queue → set stop → broadcast → join all.

* (Optionally) migrate to `std::thread` + `std::condition_variable` with a `std::function<void()>` task queue.

* **Commit:** `fix(concurrency): correct condvar usage and joined shutdown`

* **Commit:** `fix(threadpool): safe init, drain-on-shutdown, checked pthread_create`

* **Acceptance:** TSAN clean; 100 queued tasks all execute before teardown.

* **Log:** `3-threading`

---

## Phase 4 — Timers, DSP & Preallocation

4.1 **Include hygiene:** ensure `<algorithm>` for `std::clamp`, etc.

4.2 **Resample/processing**

* Pre-allocate all output buffers; avoid per-frame allocations.
* Document limits (linear interpolation acceptable at low-freq; note polyphase if >20% Nyquist; keep as comment, not code).

4.3 **Packet templates**

* Pre-build static parts of SV/GOOSE PDUs; only patch dynamic fields per frame.

* **Commit:** `perf(simd/prealloc): remove allocations in hot paths; cache templates`

* **Acceptance:** Flamegraph shows zero allocations in inner send loop.

* **Log:** `4-prealloc-dsp`

---

## Phase 5 — Encoding/Decoding Correctness Details

5.1 **IEC61850_Types Real encoding**

* Use 8 bytes for `double` and correct length.

5.2 **SampledValue indices & seqData**

* Make `indices` robust to changes; validate bounds; pre-fill `seqData` once and copy in per-frame.

5.3 **Optionals**

* Guard `.value()` calls; use `has_value()`/`value_or()`.

* **Commit:** `fix(encoding): double size, index guards, and optional checks`

* **Acceptance:** Unit tests for 127/128/255/256 sized TLVs and 8-byte Real.

* **Log:** `5-encoding-fixes`

---

## Phase 6 — Configuration Validation & Portability Flags

6.1 **Strict JSON validation**

* Required keys, ranges (e.g., `smpRate > 0`, `noChannels <= 32`, MAC format).
* Structured errors `{ok:false, msg:"..."}` or exceptions caught at the top.

6.2 **Replace `#define` globals**

* Move macros (e.g., interface names, thread counts) to `constexpr` or config.

* Interface name can be overridden by env or CLI.

* **Commit:** `feat(config): strict schema and constexpr/env overrides`

* **Acceptance:** Invalid config → clear error; IF name overridable.

* **Log:** `6-config`

---

## Phase 7 — Real-Time Foundations (code-level hooks, Linux only)

7.1 **`rt_utils.{hpp,cpp}`**

* `rt_lock_memory()`: `mlockall(MCL_CURRENT|MCL_FUTURE)`; log if fails.
* `rt_set_realtime(int prio)`: `SCHED_FIFO` on the calling thread.
* `rt_set_affinity(std::vector<int>)`: set CPU affinity (non-fatal on failure).
* `rt_sleep_abs(ns)`: `clock_nanosleep(..., TIMER_ABSTIME, ...)`, retry `EINTR`.
* Optional: open `/dev/ptpX` and expose a helper for PHC time if needed.

7.2 **Integration**

* Call rt setup early in `main()` and in critical worker threads.

* **Commit:** `feat(rt): mlockall, SCHED_FIFO, affinity, EINTR-safe sleeps`

* **Acceptance:** Threads can elevate to FIFO when allowed; memory is locked.

* **Log:** `7-rt-foundation`

---

## Phase 8 — Raw-Ethernet I/O Performance (Linux only)

8.1 **TPACKET_V3 rings**

* RX ring with `PACKET_RX_RING` (V3), optional TX ring; `bind()` to `sockaddr_ll`.
* Promisc membership; BPF filters for SV (0x88ba) and GOOSE (0x88b8), including VLAN path.
* `SIOCSHWTSTAMP` (best-effort) for HW timestamping; fallback to SW (`SO_TIMESTAMPING`).

8.2 **Fanout (optional)**

* `PACKET_FANOUT` if multiple readers.

8.3 **Qdisc bypass for TX** (optional).

* **Commit:** `feat(net): TPACKET_V3 rings, timestamping, and BPF filters`

* **Acceptance:** High PPS replay shows no drops; bounded jitter.

* **Log:** `8-packet-io`

---

## Phase 9 — Docker for Real-Time (Linux RT host)

9.1 **Dockerfile (multi-stage)**

* Build stage with toolchain; runtime stage minimal, non-root user.

9.2 **docker-compose.yml**

* For one or more Test Set instances:

  * `cpuset-cpus: "2-3"` (unique cores per service)
  * `ulimits: { rtprio: 95, memlock: -1 }`
  * `cap_add: [SYS_NICE, NET_RAW, NET_ADMIN, IPC_LOCK]`
  * `devices: ["/dev/ptp0:/dev/ptp0"]` (optional)
  * `network_mode: host` (simplest), or macvlan/SR-IOV if you choose.
  * Env/config mount for interface name, timers, etc.

9.3 **Host tuning scripts**

* `disable_gro_lro.sh`, `pin_irqs.sh`, `verify_rt_env.sh`, `cyclictest_wrap.sh`.

* **Commit:** `chore(docker): RT-ready image, compose with cpusets/caps/ulimits, and host tuning scripts`

* **Acceptance:** Inside container, SCHED_FIFO + mlock succeed; target iface visible; deterministic timing verified.

* **Log:** `9-docker-rt`

---

## Phase 10 — NIC Attachment Options (Linux host)

* **Host network** default. Optionally provide:

  * **macvlan** network creation + per-service attach.
  * **SR-IOV VF** quickstart: create VF, assign MAC/VLAN/rate; pass VF to container netns (kernel stack path) or DPDK path as a future option.

* **Docs** for each option and pitfalls (ARP/ND with macvlan, PTP device exposure, cgroup v2 RT throttling notes).

* **Commit:** `docs(net): host/macvlan/SR-IOV VF recipes`

* **Acceptance:** Operator can pick one, follow steps, and pass smoke tests.

* **Log:** `10-nic-docs`

---

## Phase 11 — macOS Portability (no raw-net; no replay)

11.1 **Platform guards**

* `src/platform/compat.hpp` defining `VTS_PLATFORM_LINUX`/`VTS_PLATFORM_MAC`.

11.2 **rt_utils on macOS**

* Compile the same file, but functions become **no-ops with INFO logs**.

11.3 **RawSocket stub**

* `raw_socket_stub.hpp` with the same public API, all methods as safe no-ops (return errors / log). Selected when `VTS_PLATFORM_MAC`.

11.4 **CLI flag/env**

* `--no-net` and/or `VTS_NO_NET=1`. On macOS, **default to no-net** unless explicitly overridden. Guard sniffer start; still run config/setup paths to exercise logic.

11.5 **CMake preset + scripts**

* `CMakePresets.json` entry `macos-dev` and scripts:

  * `scripts/build_macos.sh`
  * `scripts/run_macos_no_net.sh` → runs `--no-net` and a `--selftest` mode that instantiates modules and exits.

* **Commits:**

  * `feat(platform): compat macros; macOS rt_utils no-ops`
  * `feat(mac): RawSocket stub; default --no-net`
  * `chore(build): Apple guards and macOS preset`
  * `test(macos): selftest and no-net smoke test`

* **Acceptance:** Builds on macOS and runs `--no-net` cleanly; Linux behavior unchanged.

* **Log:** `11-macos`

---

## Phase 12 — Logging & Observability

* Lightweight logger with levels (DEBUG/INFO/WARN/ERROR), timestamps, thread id.

* Replace `std::cerr`/`printf` with centralized logging macros.

* Add counters for packet drops, parse errors, retransmits, sent frames, and timing outliers.

* **Commit:** `feat(obs): structured logging and metrics counters`

* **Acceptance:** Clear logs + counters during runs; can redirect to file.

* **Log:** `12-observability`

---

## Phase 13 — Tests & CI

* **Unit tests:** BER length edge cases (127/128/255/256), VLAN validation, MAC parser, smpCnt wrap, timer jitter boundaries (using monotonic mock/abstraction), and threadpool shutdown.

* **Integration:** (Linux) Use your existing replay tool to feed SV/GOOSE into the Test Set; assert produced outputs/behaviors if applicable.

* **Fuzz:** Tiny harness for ASN.1 decoders.

* Wire sanitizer presets to CI; run `ctest --output-on-failure`.

* **Commit:** `test: protocol units, fuzz harness, replay integration, CI presets`

* **Acceptance:** All tests pass locally & in CI; sanitizers clean.

* **Log:** `13-tests-ci`

---

## Phase 14 — Documentation

* `README-RT.md`: how to run in RT Docker on a Linux RT host (caps/ulimits/cpusets, host tuning, NIC options, PTP).

* `README-macos.md`: macOS build/run (`--no-net`), selftest, limitations.

* Troubleshooting: permissions (CAP_NET_RAW), cgroup v2 RT throttling, timestamping fallbacks, macvlan ARP/ND caveats.

* **Commit:** `docs: RT Docker guide and macOS usage`

* **Acceptance:** A new operator can deploy or dev on macOS using only the docs.

* **Log:** `14-docs`

---

## Interoperability Notes (with external peers/devices)

* Keep **GOOSE** `stNum` constant between events; increment **`sqNum`** on each retransmit (mod 2³¹).
* Ensure **Sampled Value** field sizes/ordering and endianness match IEC expectations and any partner device profiles.
* Keep a small **compatibility profile** doc listing dataset mapping, VLAN/priority, and timing parameters used by your Test Set so peers can align quickly.

---

### Definition of Done (overall)

* Repository builds cleanly on **Linux (release + sanitizers)** and **macOS (`--no-net`)**.
* **Docker/Compose** bring-up on Linux RT host succeeds with `cpuset`, `rtprio`, `memlock`, required caps, and (optionally) `/dev/ptp0`.
* Concurrency is **TSAN-clean**; socket/thread ownership is unique; BER lengths and bounds checks are correct; timers are monotonic; smpCnt wraps safely; file I/O is sanitized; logging & counters are in place; docs & scripts are included.
* `AGENT_PROGRESS_VTS.txt` contains a timestamped entry for every step.

