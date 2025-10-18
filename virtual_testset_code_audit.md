================================================================================
        VIRTUAL IEC 61850 TEST SET — COMPREHENSIVE CODE AUDIT
================================================================================

Project: Virtual Test Set / Merging Unit (IEC 61850-9-2 SV & GOOSE)
Audit Date: 2025-10-18
Auditor: AI Code Review Agent
Scope: All C++ source files (.hpp/.cpp) in src/**

================================================================================
1. EXECUTIVE SUMMARY
================================================================================

This audit identifies critical risks (P0), important improvements (P1), and
nice-to-have enhancements (P2) across all C++ source files in the Virtual
Test Set project.

--- TOP RISKS (P0 — MUST FIX) ---

1. **DATA RACE ON digital_input (Concurrency/Correctness)**
   - Global vector `digital_input` accessed by sniffer thread (write) and test
     threads (read) without synchronization
   - IMPACT: Non-deterministic trip detection; missed/phantom trip events
   - FIX: Use std::atomic<uint8_t> or mutex-protected access

2. **BUFFER OVERFLOW IN ASN.1 LENGTH PARSING (Security/Correctness)**
   - No bounds checking when reading TLV length fields in GOOSE/SV decoders
   - IMPACT: Out-of-bounds reads; potential crashes or security exploits
   - FIX: Validate all length fields against remaining buffer size

3. **MEMORY ALLOCATIONS IN REAL-TIME HOT PATH (Real-Time/Performance)**
   - std::vector growth, string operations, and dynamic allocation in send loops
   - IMPACT: Unpredictable latency spikes (>100 µs); jitter violations
   - FIX: Pre-allocate all buffers; eliminate dynamic allocation in RT paths

4. **MISSING ENDIANNESS HANDLING (Protocol Compliance)**
   - Direct bit shifts without htons/htonl on multi-byte network fields
   - IMPACT: Incorrect packet encoding on big-endian systems (rare but fatal)
   - FIX: Use htons/htonl consistently or static_assert little-endian

5. **UNVALIDATED FILE I/O IN NETWORK HANDLER (Security)**
   - TCP server writes arbitrary client-provided filenames without sanitization
   - IMPACT: Path traversal attacks; arbitrary file overwrites
   - FIX: Whitelist/sanitize filenames; use canonical paths

6. **NO ERROR HANDLING IN SOCKET CREATION (Robustness)**
   - RawSocket constructor exits on failure; no graceful degradation
   - IMPACT: Abrupt termination; poor diagnostics for permission issues
   - FIX: Return error codes or throw exceptions; propagate to caller

--- QUICK WINS (High Impact, Low Effort) ---

• Add CMake flags for warnings (-Wall -Wextra -Wpedantic -Werror)
• Replace #define constants with constexpr or enum class
• Add const-correctness to all getEncoded() methods (already mostly done)
• Enable AddressSanitizer/ThreadSanitizer in CI builds
• Add simple unit tests for protocol encoding (golden packet diffs)

--- ESTIMATED IMPACT ---

Correctness:     HIGH  (data races, buffer overflows, file path injection)
Real-Time:       HIGH  (allocations in hot path, no RT validation)
Portability:     MED   (Linux-specific APIs, no macOS fallbacks)
Security:        MED   (unvalidated inputs, raw socket permissions)
Maintainability: MED   (globals, macros, inconsistent error handling)

================================================================================
2. GLOBAL FINDINGS & CROSS-CUTTING IMPROVEMENTS
================================================================================

--- BUILD & TOOLING ---

[CMakeLists.txt — NOT REVIEWED IN DETAIL]
[Severity] P1
[Category] Build
[Issue] No global compiler warnings enabled; no sanitizers configured
[Why it matters] Silent UB, data races, and memory errors slip through
[Recommendation]
  Add to root CMakeLists.txt:
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
    option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
    option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
    if(ENABLE_ASAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    endif()
    if(ENABLE_TSAN)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    endif()
[Acceptance test] Build succeeds with -Werror; ASAN/TSAN targets available

--- LOGGING & DIAGNOSTICS ---

[Severity] P1
[Category] Maintainability
[Issue] Inconsistent error reporting: mix of std::cerr, silent failures, exits
[Why it matters] Hard to debug production issues; no centralized log levels
[Recommendation]
  1. Implement lightweight logger:
     enum LogLevel { DEBUG, INFO, WARN, ERROR };
     void log(LogLevel lvl, const char* fmt, ...);
  2. Replace all std::cerr with log(ERROR, ...) or log(WARN, ...)
  3. Compile-time stripping for DEBUG level in release builds
[Acceptance test] All errors loggable to file; log levels configurable

--- CONFIGURATION VALIDATION ---

[Severity] P1
[Category] API/Design
[Issue] JSON parsing uses exceptions but no schema validation
[Why it matters] Invalid configs crash at runtime; unclear error messages
[Recommendation]
  1. Validate required fields before parsing (e.g., channelConfig size)
  2. Check ranges (smpRate > 0, appID < 0x10000, MAC format, etc.)
  3. Return structured errors: { bool success; string error_msg; }
[Acceptance test] Invalid JSON rejected with clear diagnostic

--- TESTING STRATEGY ---

[Severity] P1
[Category] Testing
[Issue] No automated tests; manual validation only
[Why it matters] Regressions go undetected; protocol compliance unverified
[Recommendation]
  1. Golden PCAP tests: encode SV/GOOSE, compare byte-exact with reference
  2. Timing tests: measure p95/p99 inter-packet jitter under load
  3. Fuzzing harness: feed random ASN.1 to GOOSE parser (afl++ or libFuzzer)
  4. Unit tests for resample(), MAC parsing, offset calculation
[Acceptance test] >80% coverage on protocol layer; fuzz 1M iterations clean

================================================================================
3. PER-FILE FINDINGS
================================================================================

********************************************************************************
[File] src/protocols/include/Ethernet.hpp
********************************************************************************

[Lines] 16-26 (macStrToBytes)
[Severity] P1
[Category] Correctness
[Issue] Off-by-one check: "i + 2 > mac.length()" allows reading past end
[Why it matters] For MAC "AA:BB:CC:DD:EE:F", loop reads past null terminator
[Recommendation]
  Change: if (i + 2 > mac.length())
  To:     if (i + 1 >= mac.length())
  Or better: if (mac.length() != 17) throw; // Exact length check
[Snippet]
  for (size_t i = 0; i < mac.length(); i += 3) {
      if (i + 1 >= mac.length()) throw std::invalid_argument("Invalid MAC");
      bytes.push_back(static_cast<uint8_t>(std::stoi(mac.substr(i, 2), nullptr, 16)));
  }
[Acceptance test] Unit test with malformed MACs (short, long, no colons)

---

[Lines] 10-42
[Severity] P2
[Category] Performance
[Issue] getEncoded() allocates new vector on each call; not marked const
[Why it matters] Repeated allocations in hot path add jitter
[Recommendation]
  1. Mark getEncoded() as const
  2. Pre-allocate: encoded.reserve(12) before insert
  3. Consider returning const std::array<uint8_t, 12>& for zero-copy
[Acceptance test] Flamegraph shows no allocations in send loop

********************************************************************************
[File] src/protocols/include/Virtual_LAN.hpp
********************************************************************************

[Lines] 17-26 (getEncoded)
[Severity] P0
[Category] Protocol Compliance
[Issue] Priority field truncation: (priority << 13) can overflow uint16_t
[Why it matters] priority=8 yields invalid TCI; violates 802.1Q spec (3 bits)
[Recommendation]
  Add validation:
    if (priority > 7) throw std::invalid_argument("Priority must be 0-7");
    if (ID > 0xFFF) throw std::invalid_argument("VLAN ID must be 0-4095");
  Or use bit-field struct with compile-time checks
[Snippet]
  Virtual_LAN(uint8_t pri, bool dei, uint16_t id) : priority(pri), DEI(dei), ID(id) {
      if (pri > 7 || id > 0xFFF) throw std::invalid_argument("Invalid VLAN params");
  }
[Acceptance test] Reject priority=8; golden packet test with known TCI

---

[Lines] All
[Severity] P2
[Category] API/Design
[Issue] Public members without validation; mutable state
[Why it matters] Caller can set invalid values post-construction
[Recommendation] Make members private; add setters with validation
[Acceptance test] Cannot construct invalid VLAN tag

********************************************************************************
[File] src/protocols/include/IEC61850_Types.hpp
********************************************************************************

[Lines] 21-24 (UtcTime constructor)
[Severity] P1
[Category] Correctness
[Issue] Fraction conversion can overflow 32-bit intermediate
[Why it matters] For fraction > 4.29s, cast to uint64_t is too late
[Recommendation]
  Change:
    this->fraction = static_cast<uint32_t>((static_cast<uint64_t>(fraction) * (1LL << 32)) / 1000000000LL);
  To:
    this->fraction = static_cast<uint32_t>((static_cast<uint64_t>(fraction) * 4294967296ULL) / 1000000000ULL);
  (Explicit ULL avoids signed overflow; same logic but clearer)
[Acceptance test] Unit test with fraction=999999999; verify no overflow

---

[Lines] 120-240 (Data::getEncoded)
[Severity] P0
[Category] Correctness
[Issue] No length overflow checks in ASN.1 encoding
[Why it matters] Strings >255 bytes violate BER encoding; cause silent truncation
[Recommendation]
  For all string/octetString cases:
    if (value.size() > 255) throw std::overflow_error("String too long for TLV");
  For arrays/structures, handle multi-byte lengths (0x81, 0x82)
[Snippet]
  case Type::VisibleString:
      encoded.push_back(0x8A);
      if (visibleString->size() > 127) {
          encoded.push_back(0x81);
          encoded.push_back(visibleString->size() & 0xFF);
      } else {
          encoded.push_back(visibleString->size());
      }
      encoded.insert(...);
[Acceptance test] Encode 256-byte string; verify 0x81 length encoding

---

[Lines] 225 (Real encoding)
[Severity] P1
[Category] Correctness
[Issue] memcpy of double to 4-byte buffer; should be 8 bytes for double
[Why it matters] Truncates double to float; loses precision
[Recommendation]
  Change: std::vector<uint8_t> realEncoded(4);
  To:     std::vector<uint8_t> realEncoded(8);
  And update length byte accordingly
[Acceptance test] Encode π; decode; verify 15+ digit precision

---

[Lines] All
[Severity] P2
[Category] Memory Safety
[Issue] Using std::optional without value() checks in some paths
[Why it matters] Accessing .value() on empty optional is UB
[Recommendation] Use .value_or(default) or check .has_value() before access
[Acceptance test] Construct Data with unset optionals; encode safely

********************************************************************************
[File] src/protocols/include/SampledValue.hpp
********************************************************************************

[Lines] 39-47 (SampledValue constructor)
[Severity] P1
[Category] API/Design
[Issue] smpCnt passed to constructor but should be managed internally
[Why it matters] Caller responsibility unclear; easy to desync
[Recommendation] Remove smpCnt from constructor; auto-increment in getEncoded()
[Acceptance test] Encode 100 packets; smpCnt increments 0-99 correctly

---

[Lines] 128-132 (smpCnt encoding)
[Severity] P0
[Category] Correctness
[Issue] smpCnt not masked to 16 bits; can overflow into confRev field
[Why it matters] After 65535 samples, corrupts adjacent field
[Recommendation]
  Before encoding:
    smpCnt = static_cast<uint16_t>(smpCnt + 1); // Wrap at 65536
  Or in caller increment logic
[Snippet]
  _encoded.push_back(0x82); // Tag [2] INTEGER
  _encoded.push_back(2);
  uint16_t safe_cnt = smpCnt & 0xFFFF; // Explicit mask
  _encoded.push_back((safe_cnt >> 8) & 0xFF);
  _encoded.push_back(safe_cnt & 0xFF);
[Acceptance test] Increment smpCnt to 70000; verify wrap to 4464

---

[Lines] 48-53, 234-268 (indices tracking)
[Severity] P1
[Category] Correctness
[Issue] Mutable indices map; offSet calculation fragile; no bounds checks
[Why it matters] If PDU size changes, indices become invalid; OOB writes
[Recommendation]
  1. Make indices const after first getEncoded()
  2. Assert invariants: offSet + param_pos < encoded.size()
  3. Use std::span or index validation wrapper
[Acceptance test] Modify packet after encoding; detect stale indices

---

[Lines] 170-175 (seqData encoding)
[Severity] P0
[Category] Real-Time
[Issue] Hardcoded 8-byte data per channel; loop writes zeros every frame
[Why it matters] Wastes CPU in RT loop; should pre-fill once
[Recommendation]
  Pre-allocate seqData buffer in constructor; memcpy once per frame
[Snippet]
  // In constructor:
  seqDataBuffer.resize(noChannel * 8, 0);
  // In getAsduEncoded:
  indices[asdu]["seqData"] = _encoded.size();
  _encoded.push_back(0x87);
  _encoded.push_back(noChannel*8);
  _encoded.insert(_encoded.end(), seqDataBuffer.begin(), seqDataBuffer.end());
[Acceptance test] Benchmark: encode 10k packets; <1 µs per packet

---

[Lines] 96-105 (length encoding logic)
[Severity] P2
[Category] Maintainability
[Issue] Duplicated if/else for length encoding across 4 places
[Why it matters] Easy to introduce inconsistencies; hard to maintain
[Recommendation]
  Refactor to helper:
    void encodeLengthField(std::vector<uint8_t>& buf, uint16_t len) {
        if (len > 0xff) { buf.push_back(0x82); buf.push_back(len>>8); buf.push_back(len&0xFF); }
        else if (len > 0x80) { buf.push_back(0x81); buf.push_back(len); }
        else { buf.push_back(len); }
    }
[Acceptance test] Encode packets of size 127, 128, 255, 256; verify correct

********************************************************************************
[File] src/protocols/include/Goose.hpp
********************************************************************************

[Lines] 200-207 (allData encoding)
[Severity] P0
[Category] Correctness
[Issue] allDataEncoded.size() cast to uint8_t; truncates if >255 bytes
[Why it matters] For >255 data elements, length field wraps; parser fails
[Recommendation]
  Use multi-byte length encoding (same as SampledValue fix above)
[Snippet]
  uint16_t dataLen = allDataEncoded.size();
  _encoded.push_back(0xab);
  if (dataLen > 0xFF) {
      _encoded.push_back(0x82);
      _encoded.push_back((dataLen >> 8) & 0xFF);
      _encoded.push_back(dataLen & 0xFF);
  } else if (dataLen > 0x80) {
      _encoded.push_back(0x81);
      _encoded.push_back(dataLen & 0xFF);
  } else {
      _encoded.push_back(dataLen & 0xFF);
  }
[Acceptance test] Encode 300 boolean data items; verify length=0x012C

---

[Lines] 53-56 (getParamPos)
[Severity] P2
[Category] API/Design
[Issue] Returns -1 on missing key; caller must check; easy to misuse
[Why it matters] Using -1 as index causes UB or crashes
[Recommendation] Return std::optional<size_t> or throw on missing key
[Acceptance test] Query invalid param; verify no crash

---

[Lines] All (same as SampledValue.hpp)
[Severity] P1
[Category] Correctness
[Issue] Duplicate code with Protocols.hpp; inconsistent implementations
[Why it matters] Bug fixes must be applied twice; easy to miss one
[Recommendation] Remove duplication; use only Protocols.hpp or vice versa
[Acceptance test] Diff both files; ensure byte-exact encoding

********************************************************************************
[File] src/protocols/include/Protocols.hpp
********************************************************************************

[Lines] All
[Severity] P0
[Category] Code Duplication
[Issue] Protocols.hpp duplicates Ethernet, Virtual_LAN, Goose, SampledValue
[Why it matters] Changes to standalone headers not reflected here; divergence
[Recommendation]
  EITHER:
    A. Delete standalone .hpp files; use only Protocols.hpp
    B. Delete Protocols.hpp; include standalone headers
  Prefer (B) for modularity; update all #includes
[Acceptance test] Grep for "class Ethernet"; only one definition in codebase

---

[Lines] 16-523
[Severity] P2
[Category] API/Design
[Issue] Nested classes inside Protocols namespace; unusual pattern
[Why it matters] Verbose syntax (Protocols::SampledValue); non-idiomatic
[Recommendation] Use namespace Protocols { class Ethernet {...}; }
[Acceptance test] Refactor compiles; no semantic changes

********************************************************************************
[File] src/tools/include/general_definition.hpp
********************************************************************************

[Lines] 3-14
[Severity] P1
[Category] Portability
[Issue] #define constants; no type safety or scoping
[Why it matters] Global pollution; name collisions; hard to override
[Recommendation]
  Replace with:
    namespace config {
        constexpr const char* IF_NAME = "eth0";
        constexpr int SNIFFER_NO_THREADS = 1;
        constexpr int SNIFFER_NO_TASKS = 12;
        // ...
    }
[Acceptance test] Cannot redefine IF_NAME accidentally

---

[Lines] 3 (IF_NAME)
[Severity] P1
[Category] Portability
[Issue] Hardcoded "eth0"; fails on systems with different interface names
[Why it matters] macOS uses en0/en1; embedded systems vary
[Recommendation]
  1. Read from config file or environment variable
  2. Provide -D IF_NAME=<name> CMake option
  3. Runtime interface discovery (getifaddrs)
[Acceptance test] Program runs on macOS with IF_NAME=en0

********************************************************************************
[File] src/tools/include/raw_socket.hpp
********************************************************************************

[Lines] 37-48 (RawSocket constructor)
[Severity] P0
[Category] Robustness
[Issue] Constructor calls exit(1) on socket creation failure
[Why it matters] Library code should never exit; violates RAII; no cleanup
[Recommendation]
  1. Remove exit(1); throw std::runtime_error("Failed to create socket")
  2. OR: Add static factory: static std::optional<RawSocket> create()
  3. Propagate error to caller for graceful handling
[Snippet]
  socket_id = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if(socket_id < 0){
      throw std::runtime_error(std::string("Socket creation failed: ") + strerror(errno));
  }
[Acceptance test] Non-root user; program logs error and exits cleanly

---

[Lines] 50-53 (if_nametoindex)
[Severity] P1
[Category] Robustness
[Issue] if_nametoindex returns 0 on error; not checked
[Why it matters] bind() silently fails; packets never sent; hard to debug
[Recommendation]
  if_index = if_nametoindex(IF_NAME);
  if (if_index == 0) {
      throw std::runtime_error("Interface " + std::string(IF_NAME) + " not found");
  }
[Acceptance test] Invalid IF_NAME; clear error message

---

[Lines] 59-101 (setsockopt calls)
[Severity] P1
[Category] Robustness
[Issue] All setsockopt failures print error but continue
[Why it matters] RT features (QDISC_BYPASS, timestamping) silently disabled
[Recommendation]
  1. For critical options (QDISC_BYPASS), throw on failure
  2. For optional (timestamping), log warning but continue
  3. Return bitmask of enabled features
[Acceptance test] Run without CAP_NET_RAW; detect missing QDISC_BYPASS

---

[Lines] 6-19 (includes)
[Severity] P0
[Category] Portability
[Issue] All includes are Linux-specific (<linux/if_packet.h>, etc.)
[Why it matters] Fails to compile on macOS/BSD
[Recommendation]
  #ifdef __linux__
    #include <linux/if_packet.h>
    #include <linux/net_tstamp.h>
  #elif defined(__APPLE__)
    #include <net/bpf.h>
    #include <pcap/pcap.h>
  #endif
  Provide macOS implementation using BPF or libpcap
[Acceptance test] Compiles on macOS; smoke test with pcap backend

---

[Lines] 144-175 (GetMACAddress)
[Severity] P1
[Category] Correctness
[Issue] Compares ifa_name (char*) with std::string using ==
[Why it matters] Always false; should use strcmp or std::string conversion
[Recommendation]
  if (std::string(ifa->ifa_name) == interface && ifa->ifa_addr->sa_family == AF_PACKET)
[Snippet]
  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
      if (ifa->ifa_name && std::string(ifa->ifa_name) == interface) {
          // ...
      }
  }
[Acceptance test] GetMACAddress(IF_NAME) returns valid MAC string

---

[Lines] 154-158 (MAC formatting)
[Severity] P2
[Category] Portability
[Issue] Assumes AF_PACKET (Linux); macOS uses AF_LINK
[Why it matters] macOS version always returns empty string
[Recommendation] Add #ifdef for AF_LINK on macOS
[Acceptance test] Returns MAC on both Linux and macOS

********************************************************************************
[File] src/tools/include/signal_processing.hpp
********************************************************************************

[Lines] 7-32 (resample)
[Severity] P1
[Category] Performance
[Issue] Nested loop allocates new vectors; linear interpolation only
[Why it matters] Resampling 9600→4800 Hz on every test adds latency
[Recommendation]
  1. Pre-allocate output: data_resampled.reserve(data.size())
  2. For RT: use fixed-point or SIMD interpolation
  3. Consider decimation filter (low-pass) to avoid aliasing
[Snippet]
  data_resampled.reserve(data.size());
  for (const auto& signal : data) {
      resampled_signal.reserve(new_length);
      // ... (rest unchanged)
  }
[Acceptance test] Benchmark 1000 resamples; <10 ms total

---

[Lines] 24-26 (interpolation)
[Severity] P2
[Category] Correctness
[Issue] Linear interpolation introduces phase errors for high frequencies
[Why it matters] Transient waveforms may have aliasing artifacts
[Recommendation]
  Add comment: "Linear interpolation sufficient for low-freq transients;
  use polyphase filter for >20% of Nyquist"
[Acceptance test] Resample 60 Hz sine; THD < 1%

********************************************************************************
[File] src/tools/include/thread_pool.hpp
********************************************************************************

[Lines] 53-67 (ThreadPool constructor)
[Severity] P1
[Category] Concurrency
[Issue] Threads started before all members initialized
[Why it matters] Worker threads may access uninitialized taskQueue
[Recommendation]
  Initialize all members before pthread_create; use member initializer list
[Snippet]
  ThreadPool(...) : stop(false), running(true), num_tasks(no_task), 
                     front(0), rear(-1), count(0) {
      taskQueue.resize(no_task);
      pthread_mutex_init(&mutex, nullptr);
      pthread_cond_init(&not_empty, nullptr);
      pthread_cond_init(&not_full, nullptr);
      threads.resize(no_threads);
      // NOW create threads
      for (...) { pthread_create(...); }
  }
[Acceptance test] TSAN clean under high load

---

[Lines] 73-82 (destructor)
[Severity] P0
[Category] Concurrency
[Issue] Destructor sets stop=true but taskQueue may have pending tasks
[Why it matters] Tasks dropped silently; no graceful shutdown
[Recommendation]
  1. Drain queue before signaling stop
  2. OR: add shutdown() method; destructor asserts(stopped)
[Snippet]
  ~ThreadPool() {
      {
          std::lock_guard<pthread_mutex_t> lock(mutex);
          stop = true;
      }
      pthread_cond_broadcast(&not_empty);
      // Join threads...
  }
[Acceptance test] Submit 100 tasks; destroy pool; all tasks executed

---

[Lines] 138-154 (worker)
[Severity] P1
[Category] Concurrency
[Issue] Exception catch does not set running=false on throw
[Why it matters] Worker thread exits but running flag stale
[Recommendation]
  Move running=false outside try/catch or into finally-like block
[Snippet]
  static void* worker(void* arg) {
      auto* pool = static_cast<ThreadPool*>(arg);
      try {
          while(!pool->stop) {
              // ...
          }
      } catch (...) {
          std::cerr << "Exception in thread" << std::endl;
      }
      pool->running = false; // MOVE HERE
      return nullptr;
  }
[Acceptance test] Throw in task; verify worker cleans up

---

[Lines] All
[Severity] P1
[Category] API/Design
[Issue] Template on FuncType but hardcoded pthread; non-idiomatic C++
[Why it matters] Cannot use std::thread, std::function naturally
[Recommendation]
  Rewrite using std::thread, std::condition_variable, std::mutex
  Keep pthread version as legacy option with #ifdef
[Acceptance test] Drop-in replacement compiles; benchmarks equivalent

********************************************************************************
[File] src/tools/include/timers.hpp
********************************************************************************

[Lines] 21-28 (start_period with timespec)
[Severity] P2
[Category] API/Design
[Issue] Overload takes timespec but doesn't use increment_period logic
[Why it matters] Inconsistent; easy to misuse
[Recommendation] Remove overload or clarify use case in comment
[Acceptance test] Unit test both overloads; verify behavior

---

[Lines] 31-36 (wait_period)
[Severity] P0
[Category] Real-Time
[Issue] Uses CLOCK_REALTIME instead of CLOCK_MONOTONIC
[Why it matters] NTP adjustments cause time jumps; breaks RT cadence
[Recommendation]
  Change: clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL)
  Update start_period to use CLOCK_MONOTONIC as well
[Snippet]
  void start_period(long period_ns) {
      clock_gettime(CLOCK_MONOTONIC, &next_period);
      increment_period(period_ns);
  }
  void wait_period(long period_ns) {
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
      increment_period(period_ns);
  }
[Acceptance test] NTP jump during test; timing unaffected

---

[Lines] 33
[Severity] P1
[Category] Robustness
[Issue] clock_nanosleep error ignored except stderr
[Why it matters] EINTR or EINVAL goes undetected; cadence broken
[Recommendation]
  int ret = clock_nanosleep(...);
  if (ret != 0 && ret != EINTR) {
      throw std::runtime_error("clock_nanosleep failed");
  }
[Acceptance test] Inject signal; verify graceful handling

********************************************************************************
[File] src/main/include/main.hpp
********************************************************************************

[Lines] All
[Severity] P2
[Category] Code Organization
[Issue] TCPServer declared in main.hpp but defined in main.cpp
[Why it matters] Violates single responsibility; hard to reuse
[Recommendation] Move TCPServer to src/api/include/tcp_server.hpp (already exists!)
[Acceptance test] main.hpp only contains main() forward decl

********************************************************************************
[File] src/main/src/main.cpp
********************************************************************************

[Lines] 93-135 (save_file)
[Severity] P0
[Category] Security
[Issue] fileName from client used directly in file path without validation
[Why it matters] Path traversal: fileName="../../../etc/passwd" overwrites system files
[Recommendation]
  1. Validate fileName contains no ".." or "/"
  2. Use std::filesystem::canonical() to resolve path
  3. Whitelist allowed directory
[Snippet]
  #include <filesystem>
  namespace fs = std::filesystem;
  
  std::string sanitizeFileName(const std::string& name) {
      if (name.find("..") != std::string::npos || name.find("/") != std::string::npos) {
          throw std::invalid_argument("Invalid filename");
      }
      fs::path safePath = fs::path("files") / name;
      if (!safePath.string().starts_with("files/")) {
          throw std::invalid_argument("Path traversal detected");
      }
      return safePath.string();
  }
[Acceptance test] Client sends "../etc/passwd"; rejected with error

---

[Lines] 100-102 (sizeof(buffer))
[Severity] P0
[Category] Correctness
[Issue] sizeof(buffer) on pointer always returns 8 (pointer size)
[Why it matters] Reads only 8 bytes of file size instead of full buffer
[Recommendation]
  Pass maxBufferSize explicitly or use std::array with .size()
[Snippet]
  int bytesReceivedFileSize = recv(*clientSocket, buffer, maxBufferSize, 0);
[Acceptance test] Send file size "1234567890"; verify full read

---

[Lines] 152-153 (atoi)
[Severity] P1
[Category] Robustness
[Issue] atoi returns 0 on error; indistinguishable from "0" input
[Why it matters] Malformed file size causes infinite loop or buffer overflow
[Recommendation]
  Use std::stoi with exception handling:
    try {
        fileSize = std::stoi(std::string(buffer, bytesReceivedFileSize));
        if (fileSize <= 0 || fileSize > 100*1024*1024) throw;
    } catch (...) {
        send(*clientSocket, "ERROR: Invalid file size", 24, 0);
        return -1;
    }
[Acceptance test] Send fileSize="invalid"; server rejects cleanly

---

[Lines] 433-469 (test_Sniffer)
[Severity] P2
[Category] Dead Code
[Issue] Test code in production binary; uses sleep(1) for synchronization
[Why it matters] Bloats binary; timing assumptions fragile
[Recommendation] Move to separate test executable under src/tests/
[Acceptance test] Production binary <500 KB; no test functions

---

[Lines] 563-567 (main)
[Severity] P1
[Category] Robustness
[Issue] Server runs indefinitely; no signal handling for graceful shutdown
[Why it matters] SIGTERM kills process; clients disconnect abruptly
[Recommendation]
  Install SIGINT/SIGTERM handler:
    std::atomic<bool> shutdownRequested{false};
    signal(SIGINT, [](int){ shutdownRequested = true; });
    while (!shutdownRequested) { sleep(1); }
    server.stop();
[Acceptance test] Send SIGINT; server stops gracefully

********************************************************************************
[File] src/api/include/tcp_server.hpp
********************************************************************************

[Lines] All
[Severity] P2
[Category] Code Duplication
[Issue] TCPServer declared here AND in main.hpp
[Why it matters] Violates DRY; which is canonical?
[Recommendation] Keep only this file; remove from main.hpp
[Acceptance test] Single definition; main.cpp includes tcp_server.hpp

********************************************************************************
[File] src/api/src/tcp_server.cpp
********************************************************************************

[Lines] All (file contains only whitespace)
[Severity] P2
[Category] Dead Code
[Issue] Empty implementation file in build
[Why it matters] Confusing; suggests missing code
[Recommendation] Remove from CMakeLists.txt or add placeholder comment
[Acceptance test] CMake build excludes empty .cpp files

********************************************************************************
[File] src/sniffer/include/sniffer.hpp
********************************************************************************

[Lines] 32-33 (digitalInput pointer)
[Severity] P0
[Category] Concurrency
[Issue] digitalInput points to external std::vector<uint8_t> without synchronization
[Why it matters] Sniffer writes; test threads read; DATA RACE
[Recommendation]
  1. Change digitalInput to std::atomic<uint8_t>* or
  2. Protect with mutex or
  3. Use lock-free queue for updates
[Snippet]
  class SnifferClass {
      std::atomic<uint8_t>* digitalInput; // Atomic array
  };
  // In Tests_Class:
  std::vector<std::atomic<uint8_t>> digital_input;
[Acceptance test] TSAN clean; concurrent read/write does not race

---

[Lines] 41-56 (startThread)
[Severity] P1
[Category] Concurrency
[Issue] No check if thread already running; double-start UB
[Why it matters] Calling startThread twice leaks thread handle
[Recommendation]
  if (running) { std::cerr << "Already running" << std::endl; return; }
  Or: stopThread() at start of startThread()
[Acceptance test] Call startThread twice; no crash

---

[Lines] 58-63 (stopThread)
[Severity] P1
[Category] Concurrency
[Issue] Sets stop=1 then immediately joins; no memory barrier
[Why it matters] Worker may not see stop flag; hangs indefinitely
[Recommendation]
  Use std::atomic<int> stop; or add pthread_mutex lock around flag
[Snippet]
  std::atomic<int> stop{0};
  void stopThread() {
      stop.store(1, std::memory_order_release);
      if (running) pthread_join(thd, nullptr);
  }
[Acceptance test] Stop under load; thread exits within 1 second

********************************************************************************
[File] src/sniffer/src/sniffer.cpp
********************************************************************************

[Lines] 27-29 (registeredMACs global)
[Severity] P0
[Category] Concurrency
[Issue] Global vector modified by startThread; no thread safety
[Why it matters] Multiple SnifferClass instances overwrite each other's MACs
[Recommendation] Make registeredMACs a member of SnifferClass
[Acceptance test] Two sniffers with different MACs; no crosstalk

---

[Lines] 30 (sniffer global)
[Severity] P0
[Category] Concurrency
[Issue] Global SnifferClass* pointer; race if multiple instances
[Why it matters] process_GOOSE_packet uses wrong sniffer instance
[Recommendation] Pass sniffer pointer via task_arg struct
[Snippet]
  struct task_arg {
      uint8_t* pkt;
      ssize_t pkt_len;
      SnifferClass* sniffer; // ADD THIS
  };
  void process_pkt(task_arg* arg) {
      // Use arg->sniffer instead of global
  }
[Acceptance test] Two sniffers in different tests; isolated state

---

[Lines] 37-96 (process_GOOSE_packet)
[Severity] P0
[Category] Security
[Issue] No bounds checking on frame[i+j], frame[i+j+1] accesses
[Why it matters] Malformed GOOSE causes out-of-bounds read → crash/exploit
[Recommendation]
  Add: if (i+j+1 >= frameSize) return;
  Before every frame[i+j] access
[Snippet]
  while (j < length) {
      if (i+j+1 >= frameSize) {
          std::cerr << "GOOSE packet truncated" << std::endl;
          return;
      }
      uint8_t tag = frame[i+j];
      uint8_t len = frame[i+j+1];
      if (i+j+2+len > frameSize) return; // Validate data length
      // ... rest of parsing
  }
[Acceptance test] Fuzz with truncated GOOSE; no crashes

---

[Lines] 47-52 (length parsing)
[Severity] P0
[Category] Correctness
[Issue] Multi-byte length (0x82) not handled; assumes length fits in 1 byte
[Why it matters] Large GOOSE PDUs (>127 bytes) parsed incorrectly
[Recommendation]
  if (frame[i+11] == 0x82) {
      if (i+13 >= frameSize) return;
      length = (frame[i+12] << 8) | frame[i+13];
      i += 14;
  } else if (frame[i+11] == 0x81) {
      if (i+12 >= frameSize) return;
      length = frame[i+12];
      i += 13;
  } else {
      length = frame[i+11];
      i += 12;
  }
[Acceptance test] Parse GOOSE with 300-byte PDU; correct fields extracted

---

[Lines] 82-89 (boolDat parsing)
[Severity] P1
[Category] Correctness
[Issue] Only 0x83 (boolean) parsed; ignores other data types
[Why it matters] Missing integer/bitstring GOOSE data
[Recommendation]
  Handle all IEC61850_Types: 0x85 (int), 0x84 (bitstring), etc.
  Or document assumption: "Only boolean data supported"
[Acceptance test] Parse GOOSE with mixed types; extract all values

---

[Lines] 198-216 (SnifferThread recv loop)
[Severity] P1
[Category] Real-Time
[Issue] Blocking recvmsg; no timeout; cannot stop quickly
[Why it matters] stopThread waits indefinitely if no packets arrive
[Recommendation]
  Set SO_RCVTIMEO on socket (already in RawSocket but not enabled):
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000}; // 100 ms
    setsockopt(raw_socket->socket_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  Then check stop flag after timeout
[Acceptance test] Stop sniffer with no traffic; exits within 200 ms

********************************************************************************
[File] src/tests/include/tests.hpp
********************************************************************************

[Lines] 23-25 (digital_input vector)
[Severity] P0
[Category] Concurrency
[Issue] Non-atomic vector shared between threads (duplicate of sniffer.hpp)
[Why it matters] Same data race issue
[Recommendation] std::vector<std::atomic<uint8_t>> digital_input;
[Acceptance test] TSAN clean

---

[Lines] 54-61 (start_transient_test)
[Severity] P1
[Category] Robustness
[Issue] Stops existing sniffer without checking if tests are running
[Why it matters] Abruptly stops in-flight test; corrupts state
[Recommendation]
  if (is_running()) {
      std::cerr << "Test already running; stop first" << std::endl;
      return;
  }
[Acceptance test] Start test twice; second call rejected

---

[Lines] 66-70 (pthread_create)
[Severity] P1
[Category] Robustness
[Issue] pthread_create return value not checked
[Why it matters] Silent failure if resource limits exceeded
[Recommendation]
  int ret = pthread_create(&conf.thd, NULL, run_transient_test, &conf);
  if (ret != 0) {
      throw std::runtime_error("Failed to create thread: " + std::to_string(ret));
  }
[Acceptance test] Exhaust thread limit; error logged

********************************************************************************
[File] src/tests/include/transient.hpp
********************************************************************************

[Lines] 16-30 (transient_config struct)
[Severity] P1
[Category] API/Design
[Issue] Mix of configuration (fileName) and runtime state (running, stop)
[Why it matters] Unclear ownership; hard to reset for reuse
[Recommendation] Split into TransientConfig (immutable) and TransientState (mutable)
[Acceptance test] Reuse config for multiple runs; state resets cleanly

---

[Lines] 25 (RawSocket* socket)
[Severity] P2
[Category] Memory Safety
[Issue] Raw pointer to socket; no ownership semantics
[Why it matters] Unclear if transient owns socket or borrows it
[Recommendation] Use std::shared_ptr<RawSocket> or document lifetime rules
[Acceptance test] Destroy Tests_Class; transient threads clean up safely

********************************************************************************
[File] src/tests/src/tests.cpp
********************************************************************************

[Lines] 51-71 (get_transient_test_config)
[Severity] P1
[Category] Robustness
[Issue] Returns vector with single error_cfg on failure
[Why it matters] Caller must check fileloaded on every element
[Recommendation]
  Use std::optional<std::vector<transient_config>> or throw on error
[Snippet]
  std::optional<std::vector<transient_config>> get_transient_test_config(...) {
      std::ifstream f(config_path);
      if (!f.is_open()) return std::nullopt;
      // ...
  }
[Acceptance test] Invalid JSON; function returns nullopt

---

[Lines] 96-107 (Sv_packet construction)
[Severity] P0
[Category] Real-Time
[Issue] Allocates vectors in get_sampledValue_pkt_info; called per test
[Why it matters] Repeated allocations add jitter
[Recommendation] Cache Sv_packet per config; reuse across tests
[Acceptance test] Benchmark 1000 tests; no allocations after first

********************************************************************************
[File] src/tests/src/transient.cpp
********************************************************************************

[Lines] 13-14 (digital_input global)
[Severity] P0
[Category] Concurrency
[Issue] Another global digital_input pointer (third instance!)
[Why it matters] Same data race; global state chaos
[Recommendation] Remove global; pass via transient_plan struct
[Acceptance test] Grep for "digital_input"; only one definition

---

[Lines] 82-102 (getTransientData)
[Severity] P1
[Category] Robustness
[Issue] Returns empty vector on file not found; indistinguishable from empty CSV
[Why it matters] Silent failure; test runs with no data
[Recommendation]
  Throw std::runtime_error or set conf->error with message
[Snippet]
  if (data.size() < 1) {
      throw std::runtime_error("File not found: " + conf->fileName);
  }
[Acceptance test] Missing CSV; error propagated to caller

---

[Lines] 104-120 (updatePkt)
[Severity] P0
[Category] Correctness
[Issue] smpCount incremented without modulo; overflows after 65535
[Why it matters] Corrupts packet after ~13 seconds at 4800 Hz
[Recommendation]
  smpCount = (smpCount + 1) % pkt_info->smpRate;
  Or: smpCount = static_cast<uint16_t>(smpCount + 1); // Auto-wrap
[Acceptance test] Run for 1 minute at 4800 Hz; smpCnt wraps correctly

---

[Lines] 163-196 (simple_replay)
[Severity] P0
[Category] Real-Time
[Issue] while loop checks (*digital_input)[0] without memory barrier
[Why it matters] Compiler may cache value; loop never exits
[Recommendation]
  Use std::atomic or volatile (std::atomic preferred)
[Snippet]
  while (!*plan->stop && !plan->digital_input->load(std::memory_order_acquire)[0])
[Acceptance test] Sniffer sets digital_input; loop exits within 1 ms

---

[Lines] 176-179 (sendmsg in loop)
[Severity] P1
[Category] Real-Time
[Issue] No check of sizeSented; send errors ignored
[Why it matters] Buffer full or link down → silent packet loss
[Recommendation]
  if (sizeSented < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
          std::cerr << "sendmsg failed: " << strerror(errno) << std::endl;
          break;
      }
  }
[Acceptance test] Disconnect cable; error logged

---

[Lines] 185-189 (time calculation)
[Severity] P1
[Category] Correctness
[Issue] Uses CLOCK_REALTIME for trip time; NTP jumps corrupt measurement
[Why it matters] Trip time off by seconds if NTP adjusts during test
[Recommendation] Use CLOCK_MONOTONIC for all timing
[Acceptance test] Inject NTP jump; trip time accurate within 1 ms

---

[Lines] 198-232 (loop_replay)
[Severity] P2
[Category] Dead Code
[Issue] Uses CLOCK_MONOTONIC but result (time_started/ended) not stored
[Why it matters] trip_time calculation broken for loop mode
[Recommendation] Store real_time_started/ended like simple_replay
[Acceptance test] loop_flag=1; trip time computed correctly

********************************************************************************
[File] src/sampledValue/include/sv_sender.hpp
********************************************************************************

[Lines] 32-33 (constructor)
[Severity] P1
[Category] Robustness
[Issue] Constructor calls GetMACAddress; may fail silently
[Why it matters] Invalid MAC → all packets dropped
[Recommendation]
  std::string mac = GetMACAddress(IF_NAME);
  if (mac.empty()) throw std::runtime_error("Failed to get MAC");
  this->dstMac = mac;
[Acceptance test] Invalid interface; constructor throws

---

[Lines] All
[Severity] P2
[Category] Code Organization
[Issue] Config struct in header; no behavior
[Why it matters] Could be plain struct; doesn't need class
[Recommendation] Use struct or namespace for grouping
[Acceptance test] No semantic change

********************************************************************************
[File] src/sampledValue/src/sv_sender.cpp
********************************************************************************

[Lines] All (file contains only whitespace)
[Severity] P2
[Category] Dead Code
[Issue] Empty implementation file
[Recommendation] Remove from build or add comment explaining header-only
[Acceptance test] CMake excludes or documents

********************************************************************************
[File] src/goose/include/goose_receive.hpp
********************************************************************************

[Lines] All
[Severity] P2
[Category] Dead Code
[Issue] Empty header with include guards only
[Recommendation] Remove from project or add placeholder comment
[Acceptance test] Removed from build

********************************************************************************
[File] src/goose/src/goose_receive.cpp
********************************************************************************

[Lines] All (file contains only whitespace)
[Severity] P2
[Category] Dead Code
[Issue] Empty implementation file
[Recommendation] Remove from build
[Acceptance test] CMake excludes

================================================================================
4. PRIORITIZED TASK LIST
================================================================================

--- P0 (CRITICAL — FIX IMMEDIATELY) ---

1. [Concurrency] Replace std::vector<uint8_t> digital_input with 
   std::vector<std::atomic<uint8_t>> in Tests_Class; update all access sites
   with memory_order_acquire/release.
   ACCEPTANCE: TSAN clean under concurrent sniffer + test load.

2. [Security] Add bounds checking to all ASN.1 parsers (GOOSE, SV):
   - Validate i+offset < frameSize before frame[i+offset]
   - Check TLV length fields against remaining buffer
   ACCEPTANCE: Fuzz 1M malformed packets; no crashes.

3. [Security] Sanitize file paths in save_file(): reject "..", "/", non-printable.
   Use std::filesystem::canonical() and whitelist "files/" directory.
   ACCEPTANCE: Client sends "../etc/passwd"; rejected with error.

4. [Concurrency] Remove global sniffer, digital_input pointers in sniffer.cpp;
   pass via task_arg struct or make SnifferClass members.
   ACCEPTANCE: Two concurrent sniffers; isolated state.

5. [Real-Time] Fix Timer to use CLOCK_MONOTONIC instead of CLOCK_REALTIME.
   ACCEPTANCE: NTP jump during test; timing unaffected.

6. [Correctness] Mask smpCnt to 16 bits in updatePkt() and SampledValue encoding:
   smpCnt = (smpCnt + 1) % smpRate; or smpCnt = uint16_t(smpCnt + 1);
   ACCEPTANCE: Run 70000 samples; smpCnt wraps correctly.

7. [Correctness] Fix GOOSE allData length encoding to handle >255 bytes:
   use multi-byte length (0x82) when size > 0xFF.
   ACCEPTANCE: Encode 300 boolean items; parsed correctly.

8. [Portability] Wrap Linux headers in #ifdef __linux__ in raw_socket.hpp;
   add macOS stub or pcap backend.
   ACCEPTANCE: Compiles on macOS; logs "macOS support pending".

9. [Robustness] Replace exit(1) in RawSocket constructor with exception:
   throw std::runtime_error("Socket creation failed: " + strerror(errno));
   ACCEPTANCE: Non-root; clean error message, no abort.

10. [Real-Time] Pre-allocate all buffers in transient test; remove vector growth
    in send loop. Cache Sv_packet per config.
    ACCEPTANCE: Flamegraph shows zero allocations in sendmsg loop.

11. [Correctness] Fix sizeof(buffer) bug in save_file: use maxBufferSize.
    ACCEPTANCE: Transfer 100 KB file; complete reception.

12. [Protocol] Fix Virtual_LAN priority overflow: validate priority <= 7, ID <= 0xFFF.
    ACCEPTANCE: Reject VLAN(8, false, 100) with exception.

--- P1 (IMPORTANT — FIX SOON) ---

13. [Build] Add CMake compiler flags: -Wall -Wextra -Wpedantic -Werror.
    Add ASAN/TSAN options.
    ACCEPTANCE: Build with -Werror succeeds; sanitizer targets available.

14. [Logging] Implement centralized logger with levels (DEBUG/INFO/WARN/ERROR);
    replace all std::cerr with log().
    ACCEPTANCE: All errors logged; levels configurable.

15. [Config] Validate JSON schema in get_transient_test_config: check required
    fields, ranges (smpRate > 0, noChannels <= 32, etc.).
    ACCEPTANCE: Invalid config rejected with clear message.

16. [Correctness] Fix Ethernet.hpp MAC parsing off-by-one: check i+1 >= length.
    ACCEPTANCE: Malformed MACs rejected; unit test.

17. [Correctness] Fix IEC61850_Types.hpp Real encoding: allocate 8 bytes for double.
    ACCEPTANCE: Encode π; decode; 15+ digit precision.

18. [Portability] Replace #define constants with constexpr in general_definition.hpp.
    ACCEPTANCE: Cannot redefine IF_NAME accidentally.

19. [Portability] Make IF_NAME configurable (env var, CMake option, config file).
    ACCEPTANCE: Override IF_NAME at build or runtime.

20. [Robustness] Check if_nametoindex return value; throw on 0.
    ACCEPTANCE: Invalid interface; clear error.

21. [Robustness] Check pthread_create return in Tests_Class; throw on failure.
    ACCEPTANCE: Thread limit; error logged.

22. [Concurrency] Use std::atomic<int> for SnifferClass::stop; add memory barriers.
    ACCEPTANCE: stopThread() exits within 1 second.

23. [Real-Time] Add SO_RCVTIMEO to sniffer socket for non-blocking stop.
    ACCEPTANCE: Stop with no traffic; exits within 200 ms.

24. [Correctness] Handle multi-byte ASN.1 lengths (0x81, 0x82) in GOOSE parser.
    ACCEPTANCE: Parse 300-byte GOOSE PDU correctly.

25. [Performance] Optimize resample(): pre-allocate output vectors.
    ACCEPTANCE: 1000 resamples in <10 ms.

26. [Concurrency] ThreadPool: initialize all members before creating threads.
    ACCEPTANCE: TSAN clean.

27. [Robustness] ThreadPool destructor: drain queue before setting stop.
    ACCEPTANCE: Submit 100 tasks; destroy; all executed.

28. [Robustness] Add input validation to atoi() calls: use std::stoi with try/catch.
    ACCEPTANCE: Invalid fileSize rejected.

29. [Robustness] Check sendmsg return in transient loop; log errors.
    ACCEPTANCE: Cable disconnect; error logged.

30. [Correctness] Fix GetMACAddress: use std::string comparison for ifa_name.
    ACCEPTANCE: Returns valid MAC on Linux.

--- P2 (NICE-TO-HAVE — BACKLOG) ---

31. [Testing] Add unit tests for protocol encoding (golden packets).
    ACCEPTANCE: 10+ tests for SV/GOOSE; byte-exact comparison.

32. [Testing] Add fuzzing harness for ASN.1 parsers.
    ACCEPTANCE: afl++ runs 1M iterations; no crashes.

33. [Testing] Measure timing jitter: p95/p99 inter-packet gap at 4800 Hz.
    ACCEPTANCE: p99 < 50 µs on target hardware.

34. [Code Duplication] Resolve Protocols.hpp vs standalone headers; use one approach.
    ACCEPTANCE: Single definition of each protocol class.

35. [API] Return std::optional from getParamPos() instead of -1.
    ACCEPTANCE: Invalid param query; no UB.

36. [API] Split transient_config into Config (immutable) and State (mutable).
    ACCEPTANCE: Reuse config for multiple tests.

37. [Code Organization] Move test code (test_Sniffer, etc.) to separate executable.
    ACCEPTANCE: Production binary <500 KB.

38. [Robustness] Add signal handler for SIGINT/SIGTERM in main().
    ACCEPTANCE: Ctrl-C; graceful shutdown.

39. [Code Organization] Remove TCPServer duplicate definition in main.hpp.
    ACCEPTANCE: Single declaration in tcp_server.hpp.

40. [Cleanup] Remove empty .cpp files (sv_sender, goose_receive, tcp_server).
    ACCEPTANCE: CMake excludes or documents.

41. [API] Rewrite ThreadPool using std::thread, std::condition_variable.
    ACCEPTANCE: Drop-in replacement; benchmarks equivalent.

42. [Performance] Consider SIMD or fixed-point for resample().
    ACCEPTANCE: 10x speedup on resampling.

43. [Protocol] Add anti-aliasing filter to resample() for high-freq signals.
    ACCEPTANCE: 60 Hz sine; THD < 1%.

44. [Portability] Add macOS AF_LINK support to GetMACAddress.
    ACCEPTANCE: Returns MAC on macOS.

45. [Maintainability] Refactor ASN.1 length encoding into helper function.
    ACCEPTANCE: DRY; single implementation.

================================================================================
END OF AUDIT REPORT
================================================================================

SUMMARY STATISTICS:
- Total files audited: 18 (.hpp) + 7 (.cpp) = 25 files
- P0 findings: 12
- P1 findings: 18
- P2 findings: 15
- Total actionable items: 45

ESTIMATED EFFORT:
- P0 fixes: 2-3 developer-weeks
- P1 fixes: 2-3 developer-weeks
- P2 enhancements: 1-2 developer-weeks
- Total: 5-8 weeks for complete remediation

NEXT STEPS:
1. Review P0 findings with team; assign owners
2. Set up CI with ASAN/TSAN and compiler warnings
3. Create GitHub issues for each task with acceptance criteria
4. Implement P0 fixes and validate with tests
5. Iterate on P1/P2 based on priority and risk tolerance

================================================================================
