# Phase 6 Implementation - Complete Summary

## Executive Summary

✅ **Phase 6 (Config Validation & Portability) is 100% code-complete**  
📅 **Date:** October 18, 2025  
🔨 **Commits:** 1 commit (a6532eb)  
📊 **Issues Resolved:** JSON validation, #define→constexpr migration, runtime interface selection

All Phase 6 improvements have been implemented and committed. The codebase now has strict JSON schema validation with clear error messages, type-safe constexpr constants, and runtime interface configuration via environment variables.

---

## What Was Accomplished

### Phase 6.1: Strict JSON Validation

**Commit:** `a6532eb` - *"feat(config): strict JSON validation, #define→constexpr, IF_NAME env override"*

**Problem:**

- No validation of required JSON fields in configuration files
- Missing or malformed config caused crashes or undefined behavior
- No range validation for numeric parameters
- MAC address format not validated
- Silent failures with cryptic error messages

**Solution Implemented:**

#### tests.cpp - Comprehensive JSON Validation

Added strict validation to all `from_json()` functions:

**1. SampledValue_Config Validation (13 required fields)**

```cpp
void from_json(const nlohmann::json& j, SampledValue_Config& config) {
    // Required field validation
    if (!j.contains("svID")) throw std::invalid_argument("Missing required field: svID");
    if (!j.contains("smpRate")) throw std::invalid_argument("Missing required field: smpRate");
    if (!j.contains("noChannels")) throw std::invalid_argument("Missing required field: noChannels");
    if (!j.contains("confRev")) throw std::invalid_argument("Missing required field: confRev");
    if (!j.contains("smpMod")) throw std::invalid_argument("Missing required field: smpMod");
    if (!j.contains("smpSynch")) throw std::invalid_argument("Missing required field: smpSynch");
    if (!j.contains("appID")) throw std::invalid_argument("Missing required field: appID");
    if (!j.contains("macAddress")) throw std::invalid_argument("Missing required field: macAddress");
    if (!j.contains("vlanID")) throw std::invalid_argument("Missing required field: vlanID");
    if (!j.contains("vlanPriority")) throw std::invalid_argument("Missing required field: vlanPriority");
    if (!j.contains("offset")) throw std::invalid_argument("Missing required field: offset");
    if (!j.contains("scale")) throw std::invalid_argument("Missing required field: scale");
    if (!j.contains("fileName")) throw std::invalid_argument("Missing required field: fileName");
    
    // Range validation
    config.smpRate = j["smpRate"];
    if (config.smpRate <= 0) {
        throw std::invalid_argument("smpRate must be positive");
    }
    
    config.noChannels = j["noChannels"];
    if (config.noChannels <= 0 || config.noChannels > 32) {
        throw std::invalid_argument("noChannels must be between 1 and 32");
    }
    
    config.scale = j["scale"];
    if (config.scale <= 0.0) {
        throw std::invalid_argument("scale must be positive");
    }
    
    // MAC address format validation
    std::string macStr = j["macAddress"];
    if (macStr.length() != 17) {  // XX:XX:XX:XX:XX:XX
        throw std::invalid_argument("macAddress must be in format XX:XX:XX:XX:XX:XX");
    }
    for (size_t i = 0; i < macStr.length(); i++) {
        if (i % 3 == 2) {  // Colon positions: 2, 5, 8, 11, 14
            if (macStr[i] != ':') {
                throw std::invalid_argument("macAddress format error: expected ':' at position " + std::to_string(i));
            }
        } else {
            if (!std::isxdigit(macStr[i])) {
                throw std::invalid_argument("macAddress format error: invalid hex digit at position " + std::to_string(i));
            }
        }
    }
    config.macAddress = parseMacAddress(macStr);
    
    // Other fields
    config.svID = j["svID"];
    config.confRev = j["confRev"];
    config.smpMod = j["smpMod"];
    config.smpSynch = j["smpSynch"];
    config.appID = j["appID"];
    config.vlanID = j["vlanID"];
    config.vlanPriority = j["vlanPriority"];
    config.offset = j["offset"];
    config.fileName = j["fileName"];
}
```

**2. transient_config Validation (10 required fields)**

```cpp
void from_json(const nlohmann::json& j, transient_config& config) {
    // Required field validation
    if (!j.contains("file_name")) throw std::invalid_argument("Missing required field: file_name");
    if (!j.contains("file_data_fs")) throw std::invalid_argument("Missing required field: file_data_fs");
    if (!j.contains("sv_fs")) throw std::invalid_argument("Missing required field: sv_fs");
    if (!j.contains("signal_frequency")) throw std::invalid_argument("Missing required field: signal_frequency");
    if (!j.contains("digital_trigger")) throw std::invalid_argument("Missing required field: digital_trigger");
    if (!j.contains("trigger_pre_time")) throw std::invalid_argument("Missing required field: trigger_pre_time");
    if (!j.contains("trigger_pos_time")) throw std::invalid_argument("Missing required field: trigger_pos_time");
    if (!j.contains("num_samples")) throw std::invalid_argument("Missing required field: num_samples");
    if (!j.contains("num_cycles")) throw std::invalid_argument("Missing required field: num_cycles");
    if (!j.contains("num_channels")) throw std::invalid_argument("Missing required field: num_channels");
    
    // Range validation
    config.file_data_fs = j["file_data_fs"];
    if (config.file_data_fs <= 0) {
        throw std::invalid_argument("file_data_fs must be positive");
    }
    
    config.sv_fs = j["sv_fs"];
    if (config.sv_fs <= 0) {
        throw std::invalid_argument("sv_fs must be positive");
    }
    
    // Other fields
    config.file_name = j["file_name"];
    config.signal_frequency = j["signal_frequency"];
    config.digital_trigger = j["digital_trigger"];
    config.trigger_pre_time = j["trigger_pre_time"];
    config.trigger_pos_time = j["trigger_pos_time"];
    config.num_samples = j["num_samples"];
    config.num_cycles = j["num_cycles"];
    config.num_channels = j["num_channels"];
}
```

**3. Goose_info Validation (3 required fields)**

```cpp
void from_json(const nlohmann::json& j, Goose_info& info) {
    // Required field validation
    if (!j.contains("goID")) throw std::invalid_argument("Missing required field: goID");
    if (!j.contains("datSet")) throw std::invalid_argument("Missing required field: datSet");
    if (!j.contains("gocbRef")) throw std::invalid_argument("Missing required field: gocbRef");
    
    info.goID = j["goID"];
    info.datSet = j["datSet"];
    info.gocbRef = j["gocbRef"];
}
```

**Validation Rules Summary:**

| Field | Validation |
|-------|-----------|
| `smpRate` | Must be > 0 |
| `noChannels` | Must be 1-32 (practical ASDU limit) |
| `file_data_fs` | Must be > 0 |
| `sv_fs` | Must be > 0 |
| `scale` | Must be > 0 |
| `macAddress` | Exact format: `XX:XX:XX:XX:XX:XX` with hex digits |

**Benefits:**

- ✅ Clear, actionable error messages
- ✅ Early detection of config errors
- ✅ Prevents crashes from invalid data
- ✅ Validates MAC address format before parsing
- ✅ Enforces IEC 61850 constraints (e.g., channel limits)

**Error Message Examples:**

```
Missing required field: smpRate
smpRate must be positive
noChannels must be between 1 and 32
macAddress must be in format XX:XX:XX:XX:XX:XX
macAddress format error: expected ':' at position 5
macAddress format error: invalid hex digit at position 3
```

---

### Phase 6.2: #define → constexpr Migration

**Commit:** `a6532eb` (same commit)

**Problem:**

- Heavy use of `#define` macros for constants
- Not type-safe (text substitution)
- Not scoped (global namespace pollution)
- Cannot debug or inspect in debugger
- No type checking by compiler

**Solution Implemented:**

#### general_definition.hpp - Constexpr Constants

```cpp
// Before (#define macros):
#define Sniffer_NoThreads 4
#define Sniffer_NoTasks 4
#define Sniffer_ThreadPriority 80
#define Sniffer_RxSize 16
#define Protection_ThreadPriority 90
#define PORT 8080
#define MAX_CLIENTS 10

// After (constexpr constants):
// Sniffer configuration
constexpr int Sniffer_NoThreads = 4;
constexpr int Sniffer_NoTasks = 4;
constexpr int Sniffer_ThreadPriority = 80;
constexpr int Sniffer_RxSize = 16;

// Protection logic configuration
constexpr int Protection_ThreadPriority = 90;

// TCP server configuration
constexpr int PORT = 8080;
constexpr int MAX_CLIENTS = 10;
```

**Interface Name Function:**

Replaced `#define IF_NAME "eth0"` with function supporting environment override:

```cpp
// general_definition.hpp
inline const char* getInterfaceName() {
    const char* env_if = std::getenv("IF_NAME");
    return env_if ? env_if : "eth0";  // Default to eth0
}
```

**Updated Usages:**

```cpp
// raw_socket.hpp
RawSocket(const std::array<uint8_t, 6>& destMac, uint16_t protocol)
    : if_name(getInterfaceName())  // ✅ Dynamic
    , dest_mac(destMac)
    , protocol(protocol)
    , sock_fd(-1) {}

// sv_sender.hpp
class sv_sender {
private:
    std::string ifName = getInterfaceName();  // ✅ Dynamic
    // ...
};
```

**Benefits:**

- ✅ **Type Safety:** Compiler enforces `int` type
- ✅ **Scoped:** Can be namespaced, no global pollution
- ✅ **Debuggable:** Can inspect values in debugger
- ✅ **Const Correctness:** `constexpr` enforces compile-time constant
- ✅ **Better Errors:** Type mismatches caught at compile time

**Comparison:**

| Feature | `#define` | `constexpr` |
|---------|-----------|-------------|
| Type-safe | ❌ No | ✅ Yes |
| Scoped | ❌ No | ✅ Yes |
| Debuggable | ❌ No | ✅ Yes |
| Name collision | ⚠️ High risk | ✅ Low risk |
| Compile-time | ✅ Yes | ✅ Yes |
| Code size | ✅ Same | ✅ Same |

---

### Phase 6.3: Runtime Interface Selection

**Commit:** `a6532eb` (same commit)

**Problem:**

- Interface name hardcoded as `"eth0"`
- Required recompilation to change interface
- Difficult to test with different interfaces
- Not portable across systems

**Solution Implemented:**

#### getInterfaceName() with Environment Override

```cpp
// general_definition.hpp
inline const char* getInterfaceName() {
    const char* env_if = std::getenv("IF_NAME");
    return env_if ? env_if : "eth0";
}
```

**Behavior:**

1. Check `IF_NAME` environment variable
2. If set: use that interface name
3. If not set: default to `"eth0"`

**Usage Examples:**

```bash
# Default behavior (eth0)
./virtual_testset

# Override to eth1
IF_NAME=eth1 ./virtual_testset

# Override to ens192 (VMware)
IF_NAME=ens192 ./virtual_testset

# Override to enp0s3 (VirtualBox)
IF_NAME=enp0s3 ./virtual_testset

# Docker override
docker run -e IF_NAME=eno1 virtual-testset
```

**Integration Points:**

All interface name usages updated to call `getInterfaceName()`:

1. **RawSocket class** (`raw_socket.hpp`):
   ```cpp
   RawSocket(const std::array<uint8_t, 6>& destMac, uint16_t protocol)
       : if_name(getInterfaceName())  // ✅
   ```

2. **sv_sender class** (`sv_sender.hpp`):
   ```cpp
   class sv_sender {
   private:
       std::string ifName = getInterfaceName();  // ✅
   };
   ```

**Benefits:**

- ✅ No recompilation needed
- ✅ Easy testing with different interfaces
- ✅ Portable across systems (Ubuntu: eth0, RHEL: eno1, VMware: ens192)
- ✅ Docker-friendly (environment variable)
- ✅ Backwards compatible (defaults to eth0)

---

## Files Modified

### tests.cpp

- **Lines Changed:** ~120 lines across 3 `from_json()` functions
- **Purpose:** Strict JSON validation with required fields, range checks, MAC format
- **Impact:** Early error detection, clear error messages

### general_definition.hpp

- **Lines Changed:** ~15 lines
- **Purpose:** `#define` → `constexpr` migration, `getInterfaceName()` function
- **Impact:** Type safety, runtime interface selection

### raw_socket.hpp

- **Lines Changed:** ~3 lines
- **Purpose:** Use `getInterfaceName()` instead of hardcoded string
- **Impact:** Runtime interface override

### sv_sender.hpp

- **Lines Changed:** ~2 lines
- **Purpose:** Use `getInterfaceName()` for default interface
- **Impact:** Runtime interface override

---

## Testing Requirements

### Phase 6.1: JSON Validation Testing

**Test Method:** Invalid configuration files

```bash
# Test 1: Missing required field
cat > test_missing_field.json << EOF
{
  "svID": "TestSV01"
  // Missing smpRate
}
EOF

./virtual_testset --config test_missing_field.json
# Expected: "Missing required field: smpRate"

# Test 2: Invalid range
cat > test_invalid_range.json << EOF
{
  "svID": "TestSV01",
  "smpRate": -100,
  "noChannels": 50
}
EOF

./virtual_testset --config test_invalid_range.json
# Expected: "smpRate must be positive"
# Expected: "noChannels must be between 1 and 32"

# Test 3: Invalid MAC address
cat > test_invalid_mac.json << EOF
{
  "svID": "TestSV01",
  "macAddress": "01:0C:CD:01:ZZ:01"
}
EOF

./virtual_testset --config test_invalid_mac.json
# Expected: "macAddress format error: invalid hex digit at position 15"
```

**Expected Results:**

- ✅ Clear error message indicating problem
- ✅ Application exits gracefully (no crash)
- ✅ Error message includes field name and constraint

---

### Phase 6.2: Constexpr Verification

**Test Method:** Build-time checks

```bash
# Verify constexpr evaluation at compile time
# Should compile without warnings
cmake --build build

# Debugger inspection
gdb ./build/virtual_testset
(gdb) break main
(gdb) run
(gdb) print Sniffer_ThreadPriority
# Expected: 80 (should be visible in debugger)
```

**Static Analysis:**

```bash
# Check for remaining #define constants
grep -r "^#define [A-Z_]*[^_H]$" src/
# Expected: No matches (except header guards)
```

---

### Phase 6.3: Interface Override Testing

**Test Method:** Environment variable override

```bash
# Test 1: Default interface
./virtual_testset
# Expected: Uses eth0

# Test 2: Override to eth1
IF_NAME=eth1 ./virtual_testset
# Expected: Uses eth1

# Test 3: Override to non-existent interface
IF_NAME=eth99 ./virtual_testset
# Expected: Error from socket creation (interface not found)

# Test 4: Docker environment
docker run -e IF_NAME=eno1 virtual-testset
# Expected: Uses eno1 inside container
```

**Verification:**

```bash
# Monitor which interface is actually used
strace -e socket,bind ./virtual_testset 2>&1 | grep if_nametoindex
# Should show the correct interface name
```

---

## Commit Details

### Commit: a6532eb

```
feat(config): strict JSON validation, #define→constexpr, IF_NAME env override

Phase 6 complete: Config validation and portability improvements

JSON Validation:
- Added required field checks to all from_json() functions
- SampledValue_Config: 13 required fields with range validation
- transient_config: 10 required fields with range validation
- Goose_info: 3 required fields
- Range checks: smpRate > 0, noChannels 1-32, file_data_fs > 0, scale > 0
- MAC address format validation (XX:XX:XX:XX:XX:XX with hex digits)
- Clear error messages: "Missing required field: X" or "X must be positive"

Constexpr Migration:
- Replaced #define macros with constexpr int for type safety
- Sniffer_NoThreads, Sniffer_NoTasks, Sniffer_ThreadPriority, Sniffer_RxSize
- Protection_ThreadPriority, PORT, MAX_CLIENTS
- Benefits: type-safe, scoped, debuggable

Interface Override:
- IF_NAME now configurable via environment variable
- getInterfaceName() reads IF_NAME env or defaults to "eth0"
- Updated raw_socket.hpp and sv_sender.hpp to use getInterfaceName()
- Enables runtime interface selection without recompilation
- Docker-friendly: docker run -e IF_NAME=eno1
```

---

## Benefits Summary

### Configuration Robustness

- ✅ Early detection of config errors (fail-fast)
- ✅ Clear, actionable error messages
- ✅ Prevents crashes from invalid data
- ✅ MAC address format validation
- ✅ IEC 61850 constraint enforcement

### Code Quality

- ✅ Type-safe constants (`constexpr` vs `#define`)
- ✅ Scoped identifiers (no namespace pollution)
- ✅ Debuggable constants (visible in debugger)
- ✅ Const correctness enforced

### Portability

- ✅ Runtime interface selection (no recompilation)
- ✅ Environment variable override
- ✅ Docker-friendly configuration
- ✅ Cross-platform compatibility (Ubuntu, RHEL, VMware)

---

## Risk Assessment

### Low Risk Areas ✅

- JSON validation: Additive only, no behavior change for valid configs
- Constexpr: Direct replacement, same values
- Interface override: Defaults to original behavior (eth0)

### Testing Priority

1. **High Priority:** JSON validation with invalid configs (error paths)
2. **Medium Priority:** Interface override with different values
3. **Low Priority:** Constexpr (compile-time verification sufficient)

---

## Next Steps

### Immediate

- [ ] Add unit tests for JSON validation (gtest)
- [ ] Test with various invalid configs
- [ ] Verify interface override on different systems

### Phase 6 Follow-up

- [ ] Add JSON schema documentation
- [ ] Create example config files with comments
- [ ] Add config validation to CI pipeline

### Move to Phase 7

Phase 6 is code-complete. Ready to proceed with **Phase 7: Real-Time Foundations**.

---

## References

- **nlohmann/json:** JSON for Modern C++
  - Exception handling: `std::invalid_argument` for parse errors
  - Best practices: Validate before access

- **C++ constexpr:** Compile-time constants
  - Preferred over `#define` for type safety
  - Can be used in constant expressions

- **Environment Variables:** POSIX standard
  - `std::getenv()`: Thread-safe read of environment
  - Docker: `-e KEY=VALUE` to set variables

---

**Status:** ✅ **Phase 6 Complete**  
**Commit:** `a6532eb`  
**Date:** October 18, 2025
