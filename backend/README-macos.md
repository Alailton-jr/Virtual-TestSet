# macOS Development Guide

**VTS Development on macOS (No-Network Mode)**

This guide covers building and running Virtual Test Set on **macOS** for development, configuration validation, and testing. macOS does not support AF_PACKET raw sockets, so VTS runs in **no-network mode** (`--no-net`).

---

## Table of Contents

1. [Limitations](#limitations)
2. [Prerequisites](#prerequisites)
3. [Installation](#installation)
4. [Building](#building)
5. [Running](#running)
6. [Testing](#testing)
7. [Development Workflow](#development-workflow)
8. [Troubleshooting](#troubleshooting)

---

## Limitations

### What Works on macOS ✅

- **Configuration validation** - Parse and validate JSON config files
- **Self-test mode** - Instantiate all protocol modules without I/O
- **Unit tests** - Full test suite (50 tests) with Google Test
- **Logger** - Thread-safe structured logging with timestamps
- **Metrics** - Atomic counters for observability
- **Protocol encoding** - GOOSE/SV encoding (no transmission)
- **Build system** - CMake with all targets

### What Doesn't Work on macOS ❌

- **Raw sockets** - AF_PACKET not available (macOS uses BPF)
- **Packet transmission** - No `sendmsg` with `AF_PACKET`
- **Packet reception** - No sniffer thread
- **Real-time scheduling** - No `SCHED_FIFO` (macOS uses Mach threads)
- **Memory locking** - No `mlockall` (macOS has different VM system)
- **CPU affinity** - No `sched_setaffinity` (macOS uses Mach thread policy)
- **TPACKET_V3** - Linux-specific zero-copy ring buffers
- **Hardware timestamping** - Network driver feature not available
- **PTP device** - `/dev/ptp0` doesn't exist on macOS

### Use Cases for macOS

1. **Development** - Write and test code without Linux VM
2. **Configuration** - Validate JSON config files before deployment
3. **Protocol Testing** - Test encoding logic with unit tests
4. **CI/CD** - Run unit tests in GitHub Actions on macOS runners
5. **Documentation** - Build and test documentation generation

**For production deployment and full functionality, use Linux.**

---

## Prerequisites

### System Requirements

- **macOS**: 12.0+ (Monterey or later)
- **Xcode Command Line Tools**: Required for compiler and system headers
- **Homebrew**: Package manager for dependencies

### Check macOS Version

```bash
sw_vers
# ProductName: macOS
# ProductVersion: 13.0.1  (must be >= 12.0)
```

### Install Xcode Command Line Tools

```bash
# Install
xcode-select --install

# Verify
xcode-select -p
# Expected: /Library/Developer/CommandLineTools or /Applications/Xcode.app/Contents/Developer

gcc --version
# Expected: Apple clang version 14.0+
```

### Install Homebrew

```bash
# Install (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Verify
brew --version
# Expected: Homebrew 4.0+
```

---

## Installation

### Install Dependencies

```bash
# Update Homebrew
brew update

# Install CMake
brew install cmake

# Install FFTW (signal processing library)
brew install fftw

# Install nlohmann-json (JSON library)
brew install nlohmann-json

# Verify installations
cmake --version  # >= 3.14
pkg-config --modversion fftw3  # >= 3.3
brew info nlohmann-json  # Should show installed
```

### Clone Repository

```bash
# Clone VTS repository
git clone https://github.com/your-org/Virtual-TestSet.git
cd Virtual-TestSet
```

---

## Building

### Quick Build (Script)

Use the provided macOS build script:

```bash
# Build with default settings
./scripts/build_macos.sh

# Clean build (removes build directory first)
./scripts/build_macos.sh clean
```

The script will:
1. Check for macOS environment
2. Verify CMake and Xcode tools are installed
3. Clean build directory (if `clean` flag provided)
4. Configure with `macos-dev` CMake preset
5. Build with all CPU cores (`sysctl hw.ncpu`)
6. Show binary location and run commands

### Manual Build

```bash
# Configure with macOS preset
cmake --preset macos-dev

# Or manual configuration
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -GNinja

# Build
cmake --build build -j$(sysctl -n hw.ncpu)

# Verify binary
ls -lh build/vts
```

### CMake Presets

The `macos-dev` preset (in `CMakePresets.json`) is configured for macOS:

```json
{
  "name": "macos-dev",
  "displayName": "macOS Development",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "CMAKE_CXX_STANDARD": "17",
    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
    "ENABLE_ASAN": "OFF",
    "ENABLE_TSAN": "OFF",
    "ENABLE_UBSAN": "OFF"
  },
  "condition": {
    "type": "equals",
    "lhs": "${hostSystemName}",
    "rhs": "Darwin"
  }
}
```

**Note**: Sanitizers are disabled on macOS due to compatibility issues with some system libraries.

---

## Running

### Self-Test Mode

Verify that all modules instantiate correctly:

```bash
# Run self-test
./build/vts --selftest

# Expected output:
[INFO] [MAIN] Platform: macOS (no raw sockets, no real-time)
[INFO] [SELFTEST] Creating Ethernet frame...
[INFO] [SELFTEST] Creating GOOSE message...
[INFO] [SELFTEST] Creating SampledValue message...
[INFO] [SELFTEST] All modules instantiated successfully!
```

### No-Network Mode (Script)

Use the provided run script:

```bash
# Run with no-network mode
./scripts/run_macos_no_net.sh

# Run self-test via script
./scripts/run_macos_no_net.sh --selftest

# Show help
./scripts/run_macos_no_net.sh --help
```

The script will:
1. Export `VTS_NO_NET=1` environment variable
2. Set `IF_NAME=en0` (dummy interface)
3. Run VTS with `--no-net` flag
4. Show exit code and status

### Manual Execution

```bash
# Run with no-network flag
./build/vts --no-net

# Run self-test with debug logging
./build/vts --selftest --log-level DEBUG

# Run with log file
./build/vts --no-net --log-file /tmp/vts_macos.log

# View help
./build/vts --help
```

### Environment Variables

```bash
# Disable network operations (automatic on macOS)
export VTS_NO_NET=1

# Override interface name (dummy on macOS)
export IF_NAME=en0

# Set log level
export VTS_LOG_LEVEL=DEBUG

# Run
./build/vts
```

---

## Testing

### Unit Tests

VTS includes 50 unit tests that run on macOS:

```bash
# Run all tests
./build/tests/vts_tests

# Run with color output
./build/tests/vts_tests --gtest_color=yes

# Run specific test suite
./build/tests/vts_tests --gtest_filter=BEREncodingTest.*

# Verbose output
./build/tests/vts_tests --gtest_color=yes --gtest_print_time=1

# List all tests
./build/tests/vts_tests --gtest_list_tests
```

### Test Categories

1. **BER Encoding** (12 tests) - ASN.1 Basic Encoding Rules
   - Short form (≤127)
   - Long form 0x81 (128-255)
   - Long form 0x82 (256-65535)
   - Edge cases and error handling

2. **VLAN Validation** (15 tests) - 802.1Q VLAN tagging
   - Priority validation (0-7)
   - VLAN ID validation (0-4095)
   - DEI handling
   - TCI encoding

3. **MAC Parsing** (14 tests) - MAC address validation
   - Format validation (XX:XX:XX:XX:XX:XX)
   - Hex digit checks
   - Invalid format rejection

4. **smpCnt Wrap** (9 tests) - 16-bit counter wrapping
   - Wrap at 65535→0
   - Rate-based modulo
   - 70,000 sample acceptance test

### CTest Integration

```bash
# Run tests via CTest
cd build && ctest --output-on-failure

# Verbose output
cd build && ctest -V

# Run specific test
cd build && ctest -R BER_Encoding
```

### Test Results

Expected output:
```
[==========] Running 50 tests from 4 test suites.
[----------] 12 tests from BEREncodingTest (0 ms total)
[----------] 15 tests from VLANTest (0 ms total)
[----------] 14 tests from MACParserTest (0 ms total)
[----------] 9 tests from SmpCntWrapTest (1 ms total)
[==========] 50 tests from 4 test suites ran. (2 ms total)
[  PASSED  ] 50 tests.
```

See `tests/TEST_SUMMARY.md` for comprehensive test documentation.

---

## Development Workflow

### IDE Setup

#### Visual Studio Code

1. Install extensions:
   - C/C++ (Microsoft)
   - CMake Tools
   - GitLens

2. Open workspace:
   ```bash
   code .
   ```

3. Configure CMake:
   - Press `Cmd+Shift+P`
   - Select "CMake: Select a Configure Preset"
   - Choose "macos-dev"

4. Build:
   - Press `Cmd+Shift+B` (build)
   - Or use CMake Tools extension buttons

#### Xcode

Generate Xcode project:
```bash
cmake -S . -B build-xcode -G Xcode
open build-xcode/Virtual-TestSet.xcodeproj
```

### Code Editing

```bash
# Make changes to source files
vim src/protocols/include/Goose.hpp

# Rebuild
cmake --build build

# Run tests
./build/tests/vts_tests --gtest_filter=GooseTest.*
```

### Debugging

#### LLDB

```bash
# Build with debug symbols
cmake --preset macos-dev
cmake --build build

# Debug with lldb
lldb ./build/vts
(lldb) break main
(lldb) run --selftest
(lldb) continue
```

#### Visual Studio Code Debugging

Create `.vscode/launch.json`:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "VTS Self-Test",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/vts",
      "args": ["--selftest", "--log-level", "DEBUG"],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [
        { "name": "VTS_NO_NET", "value": "1" }
      ],
      "MIMode": "lldb"
    }
  ]
}
```

Press `F5` to start debugging.

### Code Formatting

```bash
# Install clang-format
brew install clang-format

# Format all C++ files
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting (dry run)
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run --Werror
```

---

## Troubleshooting

### Build Errors

#### CMake Not Found

```
zsh: command not found: cmake
```

**Solution**: Install CMake via Homebrew:
```bash
brew install cmake
```

#### Compiler Not Found

```
No CMAKE_CXX_COMPILER could be found
```

**Solution**: Install Xcode Command Line Tools:
```bash
xcode-select --install
```

#### FFTW Not Found

```
Could NOT find fftw3 (missing: FFTW3_LIBRARIES FFTW3_INCLUDE_DIRS)
```

**Solution**: Install FFTW via Homebrew:
```bash
brew install fftw
```

#### JSON Library Not Found

```
Could NOT find nlohmann_json (missing: nlohmann_json_DIR)
```

**Solution**: Install nlohmann-json:
```bash
brew install nlohmann-json
```

### Runtime Errors

#### Network Operations Attempted

```
ERROR: Raw socket operations not supported on macOS
```

**Expected**: macOS doesn't support AF_PACKET. This is normal. Use `--no-net`:
```bash
./build/vts --no-net
```

#### Environment Variable Not Set

```
WARN: VTS_NO_NET not set, network operations may fail
```

**Solution**: Export environment variable:
```bash
export VTS_NO_NET=1
./build/vts
```

Or use the provided script:
```bash
./scripts/run_macos_no_net.sh
```

#### Self-Test Failures

```
ERROR: Module instantiation failed
```

**Debug**:
```bash
# Run with debug logging
./build/vts --selftest --log-level DEBUG

# Check for specific errors
./build/vts --selftest 2>&1 | grep ERROR
```

### Test Failures

#### Tests Not Found

```
./build/tests/vts_tests: No such file or directory
```

**Solution**: Tests weren't built. Rebuild with tests target:
```bash
cmake --build build --target vts_tests
```

#### Test Crashes

```
Segmentation fault: 11
```

**Debug**:
```bash
# Run under lldb
lldb ./build/tests/vts_tests
(lldb) run --gtest_filter=FailingTest
(lldb) bt  # backtrace after crash
```

---

## Platform Differences

### Key Differences from Linux

| Feature | Linux | macOS |
|---------|-------|-------|
| Raw Sockets | `AF_PACKET` | BPF (not used in VTS) |
| RT Scheduling | `SCHED_FIFO` | Mach thread policy |
| Memory Locking | `mlockall` | `mlock` (different API) |
| CPU Affinity | `sched_setaffinity` | `thread_policy_set` |
| Network Timestamping | `SO_TIMESTAMPING` | Not available |
| Timer | `clock_nanosleep` | `nanosleep` (relative only) |

### Code Platform Guards

VTS uses platform detection macros:

```cpp
#ifdef VTS_PLATFORM_LINUX
    // Linux-specific code (AF_PACKET, SCHED_FIFO)
#elif defined(VTS_PLATFORM_MAC)
    // macOS-specific code (stubs, no-ops with logging)
#else
    #error "Unsupported platform"
#endif
```

See `src/platform/include/compat.hpp` for details.

### Stub Implementations

macOS uses stub implementations that log operations but don't perform them:

- **RawSocket** - Logs socket creation, returns dummy values
- **rt_lock_memory** - Logs "not available on macOS", returns false
- **rt_set_realtime** - Logs priority value, returns false
- **rt_set_affinity** - Logs CPU list, returns false
- **rt_sleep_abs** - Uses `nanosleep` (relative time) as fallback

All stubs are safe and allow VTS to compile and run in no-net mode.

---

## Continuous Integration

### GitHub Actions

VTS CI runs on macOS runners:

```yaml
name: macOS CI

on: [push, pull_request]

jobs:
  build-macos:
    runs-on: macos-latest
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          brew install cmake fftw nlohmann-json
      
      - name: Build
        run: |
          cmake --preset macos-dev
          cmake --build build -j$(sysctl -n hw.ncpu)
      
      - name: Run self-test
        run: |
          ./build/vts --selftest
      
      - name: Run unit tests
        run: |
          ./build/tests/vts_tests --gtest_output=xml:test_results.xml
      
      - name: Upload test results
        uses: actions/upload-artifact@v3
        if: always()
        with:
          name: test-results
          path: test_results.xml
```

### Local CI Simulation

Simulate CI build locally:

```bash
# Clean environment
./scripts/build_macos.sh clean

# Build
./scripts/build_macos.sh

# Run self-test
./scripts/run_macos_no_net.sh --selftest

# Run unit tests
./build/tests/vts_tests --gtest_output=xml:test_results.xml

# Check exit code
echo $?  # Should be 0
```

---

## Migration to Linux

When moving from macOS development to Linux deployment:

1. **Remove no-net flag**:
   ```bash
   # macOS
   ./build/vts --no-net
   
   # Linux
   sudo ./build/vts  # Network mode enabled by default
   ```

2. **Configure interface**:
   ```bash
   # macOS (dummy)
   export IF_NAME=en0
   
   # Linux (real interface)
   export IF_NAME=eth0
   ```

3. **Enable RT features**:
   - RT kernel (PREEMPT_RT)
   - CPU isolation
   - Memory locking
   - SCHED_FIFO scheduling

   See [README-RT.md](README-RT.md) for Linux RT setup.

4. **Test packet operations**:
   - Use `tcpdump` to verify packet transmission
   - Check GOOSE/SV reception with sniffer
   - Validate with Wireshark

5. **Performance tuning**:
   - Disable GRO/LRO
   - Pin IRQs
   - Set CPU governor to performance
   - Run cyclictest for latency measurement

---

## References

- [Main README](README.md) - Project overview and features
- [README-RT.md](README-RT.md) - Linux RT deployment guide
- [tests/README.md](tests/README.md) - Unit test documentation
- [Homebrew](https://brew.sh/) - macOS package manager
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [LLDB Debugging](https://lldb.llvm.org/use/tutorial.html)

---

**Last Updated**: Phase 14 - November 3, 2025
