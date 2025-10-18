# Prompt for Agent — Full Code Audit (C++ .cpp/.hpp) → `virtual_testset_code_audit.txt`

## Context

You are auditing a high-performance, multi-threaded C++ project that emulates a **Virtual IEC 61850 Test Set / Merging Unit**. It streams IEC 61850-9-2 **Sampled Values (SV)** and listens to **GOOSE** messages to measure trip time with high precision via a stimulus-response loop.

**Repo tree (reference):**

```
.
├── all_code.txt
├── all-files.py
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
├── Makefile
├── repo-analyze.md
└── src
    ├── api
    │   ├── CMakeLists.txt
    │   ├── include/tcp_server.hpp
    │   └── src/tcp_server.cpp
    ├── goose
    │   ├── CMakeLists.txt
    │   ├── include/goose_receive.hpp
    │   └── src/goose_receive.cpp
    ├── main
    │   ├── CMakeLists.txt
    │   ├── include/main.hpp
    │   └── src/main.cpp
    ├── protocols
    │   ├── CMakeLists.txt
    │   └── include
    │       ├── Ethernet.hpp
    │       ├── Goose.hpp
    │       ├── IEC61850_Types.hpp
    │       ├── Protocols.hpp
    │       ├── SampledValue.hpp
    │       └── Virtual_LAN.hpp
    ├── sampledValue
    │   ├── CMakeLists.txt
    │   ├── include/sv_sender.hpp
    │   └── src/sv_sender.cpp
    ├── sniffer
    │   ├── CMakeLists.txt
    │   ├── include/sniffer.hpp
    │   └── src/sniffer.cpp
    ├── tests
    │   ├── CMakeLists.txt
    │   ├── include/tests.hpp
    │   └── include/transient.hpp
    │   └── src/tests.cpp
    │   └── src/transient.cpp
    └── tools
        ├── CMakeLists.txt
        └── include
            ├── general_definition.hpp
            ├── raw_socket.hpp
            ├── signal_processing.hpp
            ├── thread_pool.hpp
            └── timers.hpp
```

**High-level behavior (for guidance):**

* Reads transient CSV waveforms → resamples → streams SV with precise `smpCnt` cadence on raw socket.
* Listens for GOOSE; parses ASN.1; maps trip bits; stops SV stream; records trip time.
* Remote TCP API receives configs and commands (save files/configs, start/stop tests, fetch results).
* Protocol layer models Ethernet/VLAN/SV/GOOSE with ASN.1 encoding and dynamic field offsets.

## Objective

Perform a **comprehensive, file-by-file audit of every `.cpp` and `.hpp`**, and produce a single, actionable report:

* Save as **`virtual_testset_code_audit.txt`** at the repo root.
* For **each file**, list concrete **fixes and improvements** with rationale and how to implement.

If filesystem access is limited, use `all_code.txt` as a fallback source of truth.

## Deliverable: TXT Report Format

Write a single text file with this structure:

1. **Executive Summary (1–2 pages)**

* Top risks (P0) that could break correctness, timing, or safety.
* Quick wins (high impact, low effort).
* Estimated impact on: correctness, real-time timing/jitter, portability (Linux/macOS), security, maintainability.

2. **Global Findings & Cross-Cutting Improvements**

* Build/tooling/CMake, warnings/sanitizers, logging, error handling, config validation, testing strategy.

3. **Per-File Findings** (repeat for *every* .hpp/.cpp)
   Use this **finding card** template for each issue:

```
[File] <relative/path.hpp|.cpp>
[Lines] <approx lines / symbol names>
[Severity] P0 (critical) | P1 (important) | P2 (nice-to-have)
[Category] (Correctness | Concurrency | Real-Time | Performance | Memory Safety | Network/Protocols | API/Design | Portability | Style)
[Issue] <what is wrong/fragile/unclear>
[Why it matters] <consequence; concrete failure modes>
[Recommendation] <exact change(s): signatures, types, algorithms, RAII, checks>
[Snippet/Pointer] <brief excerpt or symbol to locate it>
[Acceptance test] <unit/integration check or measurable criterion>
```

4. **Prioritized Task List**

* A single ordered list of tasks the team can execute (P0→P2), each with a short acceptance criterion.

## Scope & Depth Checklist (what to look for)

### A. Correctness & Protocol Compliance

* **Endianness & field widths:** ensure `htons/htonl` on network fields; verify Ethertype (SV=0x88BA, GOOSE=0x88B8), VLAN tags (802.1Q) and priority bits.
* **ASN.1 TLV parsing:** strict bounds checking; handle length overflows/indefinite lengths; reject malformed payloads; avoid unchecked pointer arithmetic.
* **Packet building:** avoid writing past buffer; prefer `std::byte`, `std::array`, `std::span` over raw pointers; no `reinterpret_cast` to packed structs over untrusted buffers.
* **Dynamic offsets:** verify offset tracking for `smpCnt` and payload regions; assert invariants on every send.

### B. Concurrency & Real-Time

* **Data races:** shared `digital_input` or flags must be `std::atomic<T>` or guarded by mutex; use correct memory order.
* **Thread priorities & timers:** use monotonic clock sources; confirm `clock_nanosleep`/`timerfd` usage (Linux) and graceful fallbacks; no busy-waits in RT loop.
* **No allocations in the hot path:** pre-allocate buffers, pre-compute headers; avoid `new`, `std::string` growth, or I/O in the send loop.
* **Thread pool correctness:** graceful shutdown; bounded work queues; no dangling references/captures.

### C. Performance

* Batch system calls where possible, use `sendmmsg`/`recvmmsg` (Linux), pinned cores/affinity (optional), cache-friendly data layouts, avoid needless copies; consider `mlockall`/hugepages (if applicable).

### D. Network I/O & Security

* **Raw socket RAII wrappers:** guarantee close on all paths; propagate errors.
* **Interface selection & permissions:** clear diagnostics for missing CAP_NET_RAW/root.
* **BPF/pcap filters:** minimize capture overhead when sniffing; drop non-GOOSE frames earlier.
* **Input validation:** JSON/CSV schema checks with clear error messages; reject out-of-range sample rates.

### E. API & Design Quality

* **Const-correctness, `noexcept`, spans, views**; prefer `enum class`; avoid macros for constants; centralize configuration types; remove hidden globals.
* **Logging:** lightweight, leveled logger (compile-time stripping for RT paths), ring buffer for high-rate debug.
* **Error handling:** no silent `catch(...)`; return `expected`-style results or status codes; avoid UB.

### F. Portability (Linux/macOS)

* Isolate Linux-specific code (raw sockets, `clock_nanosleep`) behind compile-time switches; provide macOS fallbacks (e.g., `pcap` for capture, `bpf` sockets) and feature-gated paths.

### G. Build & Tooling

* **CMake:** global warnings `-Wall -Wextra -Wpedantic`, treat warnings as errors in CI; options for `ASAN/TSAN/UBSAN`; `-fno-exceptions` only if audited; LTO/RelWithDebInfo; `PIC` as needed.
* **Static analysis:** configure `clang-tidy` with checks for concurrency, performance, modernize, bugprone; `cppcheck` as secondary signal.

### H. Testing Strategy (Design Suggestions)

* **Golden PCAP tests** (SV & GOOSE) to verify byte-exact encodings.
* **Timing tests**: jitter/latency bounds under load (record real send intervals).
* **Fuzzing harness** for ASN.1 decoders (afl++/libFuzzer) with length/pathological inputs.

## Process

1. **Inventory**

   * Enumerate all `.hpp` and `.cpp` under `src/**` recursively. If executable access is unavailable, use `all_code.txt`.
   * For each file, note public APIs, ownership/RAII, thread interactions, I/O, and hot paths.

2. **Static Reasoning (mandatory)**

   * Manually analyze each file and produce *at least 3 actionable findings* or explicitly mark “No issues found — rationale: …”.
   * Cross-link globals/singletons across files and flag hazards.

3. **Standards/Domain Verification (mandatory)**

   * For SV/GOOSE structures, verify fields and lengths against IEC-61850 expectations; flag any ambiguous or magic numbers and recommend named constants & references.

4. **Tooling Suggestions (report-only)**

   * Even if you cannot run tools, recommend exact `clang-tidy` checks and CMake targets to add (show code blocks).

5. **Prioritization**

   * Rank all findings P0/P1/P2 with clear acceptance tests. P0/P1 must be concrete and minimally invasive where possible.

## Output Requirements

* **Single file:** `virtual_testset_code_audit.txt` in the repo root.
* Use clear section headers exactly as specified above.
* For **every** `.hpp/.cpp`, include at least one finding card or a “No issues found” note with justification.
* Use concise code excerpts where helpful (avoid long dumps).
* End with the **Prioritized Task List** (each task ≤3 lines with an acceptance criterion).

## Tone & Constraints

* Be specific, mechanical, and implementation-oriented. Avoid generic advice.
* When proposing changes, show exact signatures, types, or small code sketches.
* If something is uncertain (e.g., platform behavior), state the assumption and the safest default.

## Examples of High-Value Findings (use as models)

* **Concurrency / P0:** “`digital_input` is written by sniffer thread and read by test engine without synchronization → data race. **Fix:** change to `std::atomic<bool> digital_input[N]` with `memory_order_relaxed` for writes and `acquire` on reads (or mutex). **Acceptance:** TSAN-clean; deterministic trip edge detection under stress.”
* **Protocol / P0:** “`smpCnt` not masked to 16-bit range; risk of overflow into next field. **Fix:** increment as `smpCnt = static_cast<uint16_t>(smpCnt + 1u);` and write via network order. **Acceptance:** Golden packet diff remains valid after 100k samples.”
* **Real-Time / P1:** “Memory allocation in send loop (`std::vector` growth). **Fix:** pre-reserve and reuse buffers; move logging out of hot path. **Acceptance:** p95 inter-packet gap within ±5 µs at 4.8 kHz on target.”

---

**Action:** Perform the audit now and write the complete report to `virtual_testset_code_audit.txt` following the format and requirements above.
