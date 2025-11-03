# Virtual Test Set (VTS)

**Real-Time IEC 61850 GOOSE & Sampled Values Test Generator**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](BUILD_STATUS.md)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-blue)](README-macos.md)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## Overview

Virtual Test Set is a **high-performance, real-time** network test generator for **IEC 61850-9-2 Sampled Values (SV)** and **IEC 61850-8-1 GOOSE** protocols. Designed for:

- **Protection relay testing** - Generate transient fault scenarios with microsecond precision
- **IED integration testing** - Validate GOOSE/SV interoperability across vendors
- **Network performance testing** - High-rate packet generation with hardware timestamping
- **Protocol conformance** - IEC 61850 compliant encoding with BER/ASN.1 validation

### Key Features

✅ **Real-Time Performance**
- SCHED_FIFO scheduling with priority 80-90
- Memory locking (`mlockall`) to prevent paging
- CLOCK_MONOTONIC timers immune to NTP adjustments
- Zero-copy packet I/O with TPACKET_V3 ring buffers (Linux)

✅ **IEC 61850 Protocol Support**
- **GOOSE**: Multicast control messages with retransmission logic
- **SV**: 4800/9600 Hz sampled data with 16-bit `smpCnt` wrapping
- Full BER/ASN.1 encoding with 0x81/0x82 length forms
- VLAN 802.1Q tagging with priority validation

✅ **Advanced Testing**
- Transient fault injection (voltage/current waveforms from CSV)
- Digital input triggering for event sequences
- Configurable phase shift, magnitude, and sampling rates
- JSON configuration for reproducible test scenarios

✅ **Cross-Platform**
- **Linux**: Full real-time mode with AF_PACKET raw sockets
- **macOS**: Development mode with `--no-net` for config validation
- Docker deployment with RT kernel support

✅ **Observability**
- Structured logging with thread-safe output and microsecond timestamps
- Metrics system (packet drops, parse errors, timing outliers)
- Google Test unit tests (50 tests: BER encoding, VLAN, MAC parsing, smpCnt)
- Sanitizer support (ASAN/TSAN/UBSAN) for CI/CD

---

## Quick Start

### Prerequisites

**Linux** (recommended for production):
- Ubuntu 22.04+ or RHEL 8+
- RT kernel (PREEMPT_RT) for deterministic performance
- Network interface with raw socket support
- CMake 3.14+, GCC 11+
- Dependencies: `libfftw3-dev`, `nlohmann-json3-dev`

**macOS** (development only):
- macOS 12+ (Monterey or later)
- Xcode Command Line Tools
- CMake 3.14+
- Dependencies: `fftw` (via Homebrew)

### Build (Linux)

```bash
# Clone repository
git clone https://github.com/your-org/Virtual-TestSet.git
cd Virtual-TestSet

# Install dependencies (Ubuntu)
sudo apt-get install -y cmake build-essential libfftw3-dev nlohmann-json3-dev

# Build with strict warnings
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run self-test (verify modules instantiate)
./build/vts --selftest

# Run with network interface
sudo ./build/vts --log-level INFO
```

### Build (macOS)

```bash
# Install dependencies
brew install cmake fftw nlohmann-json

# Build with macOS preset
./scripts/build_macos.sh

# Run in no-network mode (config validation)
./scripts/run_macos_no_net.sh --selftest
```

See [README-macos.md](README-macos.md) for detailed macOS instructions.

### Docker Deployment (Linux RT Host)

```bash
# Build Docker image
docker build -t vts:latest .

# Run with host networking and RT capabilities
docker-compose up -d

# View logs
docker-compose logs -f vts

# Check metrics
docker-compose exec vts cat /app/logs/metrics.json
```

See [README-RT.md](README-RT.md) for comprehensive RT deployment guide.

---

## Usage

### Command-Line Options

```bash
./vts [OPTIONS]

Options:
  --help, -h              Show help message
  --selftest              Run module self-test and exit
  --no-net                Disable network operations (config validation only)
  --enable-net            Override platform default (force network mode)
  --log-level LEVEL       Set log level: DEBUG, INFO, WARN, ERROR, NONE (default: INFO)
  --log-file PATH         Write logs to file (default: stdout/stderr only)

Environment Variables:
  IF_NAME                 Network interface name (default: eth0 on Linux, en0 on macOS)
  VTS_NO_NET              Disable network operations: "1" or "true"
  VTS_LOG_LEVEL           Log level override
  VTS_LOG_FILE            Log file path override
  RT_PRIORITY             Real-time thread priority 1-99 (Linux only, default: 80/90)
  RT_CPU_AFFINITY         CPU affinity mask for pinning (Linux only, e.g., "2,3")
```

### Configuration Files

VTS uses JSON configuration files for test scenarios:

**SV Configuration** (`sv_config.json`):
```json
{
  "MAC_SRC": "01:0C:CD:04:00:01",
  "MAC_DST": "01:0C:CD:04:00:02",
  "APPID": 16384,
  "VLAN_ID": 100,
  "VLAN_PRIORITY": 4,
  "smpRate": 4800,
  "noChannels": 8,
  "confRev": 1,
  "smpMod": 0,
  "smpSynch": 1
}
```

**Transient Configuration** (`transient_config.json`):
```json
{
  "file_data": "fault_waveform.csv",
  "file_data_fs": 12800,
  "file_offset": 0,
  "scale": 1000.0,
  "prefault_time": 0.5,
  "transient_duration": 1.0,
  "phase_shift_degrees": [0, 120, 240],
  "active": true
}
```

**GOOSE Configuration** (`goose_config.json`):
```json
{
  "gocbRef": "IED1$GO$GCB1",
  "datSet": "IED1$DATASET1",
  "goID": "GOOSE1"
}
```

See `tests/README.md` for full configuration schema.

### Running Tests

```bash
# Run all unit tests
./build/tests/vts_tests

# Run specific test suite
./build/tests/vts_tests --gtest_filter=BEREncodingTest.*

# Run with sanitizers
cmake -S . -B build -DENABLE_ASAN=ON
cmake --build build
./build/tests/vts_tests

# Run via CTest
cd build && ctest --output-on-failure
```

See `tests/README.md` and `tests/TEST_SUMMARY.md` for comprehensive testing guide.

---

## Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         VTS Core                             │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Sniffer    │  │ Transient    │  │  TCP API     │      │
│  │   Thread     │  │ Test Thread  │  │  Server      │      │
│  │ (Priority 80)│  │ (Priority 90)│  │              │      │
│  └───────┬──────┘  └──────┬───────┘  └──────┬───────┘      │
│          │                 │                  │              │
│          ▼                 ▼                  ▼              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         Protocol Layer (GOOSE / SV Encoders)         │   │
│  │  • BER/ASN.1 Encoding    • VLAN 802.1Q              │   │
│  │  • Ethernet Framing      • IEC 61850 Types          │   │
│  └──────────────────────────────────────────────────────┘   │
│          │                                                   │
│          ▼                                                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │       Packet I/O Layer (Platform Abstraction)        │   │
│  │  • Linux: AF_PACKET + TPACKET_V3 (zero-copy)        │   │
│  │  • macOS: Stub (no-net mode)                        │   │
│  │  • BPF filtering     • HW timestamps                 │   │
│  └──────────────────────────────────────────────────────┘   │
│          │                                                   │
│          ▼                                                   │
│     Network Interface (eth0/en0)                             │
└─────────────────────────────────────────────────────────────┘
```

### Module Responsibilities

- **Sniffer** (`src/sniffer/`) - Receives GOOSE messages, parses with bounds checking
- **Transient** (`src/tests/`) - Generates SV packets with fault waveforms from CSV
- **API** (`src/api/`) - TCP server for remote control and metrics retrieval
- **Protocols** (`src/protocols/`) - IEC 61850 encoding/decoding (Goose, SampledValue, Ethernet, VLAN)
- **Tools** (`src/tools/`) - ThreadPool, signal processing, timers, logger, metrics, RT utilities
- **Platform** (`src/platform/`) - Cross-platform abstraction (compat.hpp, raw_socket stubs)

### Threading Model

1. **Main Thread** - TCP server accept loop, configuration loading
2. **Sniffer Threads** (1+ per interface) - GOOSE packet reception with SCHED_FIFO:80
3. **Transient Test Thread** - SV packet transmission with SCHED_FIFO:90
4. **ThreadPool** - Background tasks (file I/O, metrics aggregation)

All threads are **joinable** (no detach), use **pthread condition variables** with predicates, and have **graceful shutdown** semantics.

---

## Real-Time Performance

### Linux RT Kernel

For deterministic microsecond-level timing, use a **PREEMPT_RT** patched kernel:

```bash
# Check current kernel config
zcat /proc/config.gz | grep PREEMPT

# Expected output for RT kernel:
CONFIG_PREEMPT_RT=y
CONFIG_PREEMPT_RT_FULL=y
```

**Installation**:
- Ubuntu: Install `linux-image-rt-amd64` package
- Compile from source: Apply RT patches from [kernel.org/pub/linux/kernel/projects/rt](https://kernel.org/pub/linux/kernel/projects/rt/)

### Host System Tuning

```bash
# Disable power management
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Isolate CPUs for RT threads (add to kernel cmdline)
# /etc/default/grub: GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
sudo update-grub && sudo reboot

# Disable GRO/LRO for low latency
sudo ethtool -K eth0 gro off lro off gso off tso off

# Pin network IRQs to non-RT CPUs
sudo ./scripts/pin_irqs.sh eth0 0,1
```

See [README-RT.md](README-RT.md) for comprehensive RT tuning guide.

### Performance Metrics

Measured on Intel Xeon E5-2680 v4 @ 2.4 GHz with RT kernel:

| Metric | Value |
|--------|-------|
| SV packet generation rate | 9600 Hz sustained |
| GOOSE retransmission jitter | <10 µs (99th percentile) |
| Packet processing latency | <50 µs (median) |
| Max CPU usage (4 cores) | ~35% at 9600 Hz |
| Memory footprint | <100 MB RSS |
| Cyclictest max latency | <50 µs with isolcpus |

---

## Testing

VTS includes comprehensive test coverage:

- **Unit Tests** (50 tests, 100% passing):
  - BER encoding (short form, 0x81/0x82 long form, edge cases)
  - VLAN validation (priority, ID, DEI, TCI encoding)
  - MAC address parsing (format validation, hex checks)
  - smpCnt wrapping (16-bit overflow, 70k sample test)

- **Integration Tests** (manual):
  - Packet replay with pcap files
  - Sniffer → Parser → Protocol stack validation
  - Multi-threaded stress testing

- **Sanitizer Testing**:
  - AddressSanitizer (ASAN) - memory safety
  - ThreadSanitizer (TSAN) - concurrency correctness
  - UndefinedBehaviorSanitizer (UBSAN) - UB detection

Run tests with:
```bash
cd build && ctest --output-on-failure
```

See `tests/TEST_SUMMARY.md` for detailed test documentation.

---

## Troubleshooting

### Common Issues

**1. Permission Denied (socket creation)**
```
ERROR: Failed to create raw socket: Operation not permitted
```
**Solution**: Run with `sudo` or grant `CAP_NET_RAW` capability:
```bash
sudo setcap cap_net_raw=ep ./build/vts
```

**2. SCHED_FIFO Priority Denied**
```
WARN: Failed to set SCHED_FIFO: Operation not permitted
```
**Solution**: Run with `sudo` or configure `/etc/security/limits.conf`:
```
* soft rtprio 95
* hard rtprio 95
```

**3. cgroup v2 RT Throttling**
```
ERROR: pthread_create failed: Resource temporarily unavailable
```
**Solution**: Disable cgroup RT throttling in `/etc/docker/daemon.json`:
```json
{
  "cpu-rt-runtime": -1
}
```
See [README-RT.md](README-RT.md#cgroup-v2-rt-throttling) for details.

**4. macOS Network Operations**
```
ERROR: Raw socket operations not supported on macOS
```
**Expected**: macOS doesn't support AF_PACKET. Use `--no-net` mode:
```bash
./build/vts --no-net --selftest
```
See [README-macos.md](README-macos.md) for macOS limitations.

**5. Missing Interface**
```
ERROR: if_nametoindex failed: No such device (eth0)
```
**Solution**: Specify correct interface with `IF_NAME` environment variable:
```bash
IF_NAME=enp0s25 ./build/vts
```

### Debug Logging

Enable verbose logging for troubleshooting:
```bash
./build/vts --log-level DEBUG --log-file /tmp/vts_debug.log
```

Log tags for filtering:
- `MAIN` - Startup, platform, TCP server
- `RT` - Real-time initialization (mlockall, SCHED_FIFO, affinity)
- `GOOSE` - GOOSE parsing with frame context
- `SNIFFER` - Packet reception, socket operations
- `TEST` - Transient test execution
- `FILE` - Configuration file I/O

---

## Platform Support

| Feature | Linux | macOS |
|---------|-------|-------|
| Raw sockets (AF_PACKET) | ✅ | ❌ |
| Real-time scheduling (SCHED_FIFO) | ✅ | ❌ |
| Memory locking (mlockall) | ✅ | ❌ |
| CPU affinity | ✅ | ❌ |
| TPACKET_V3 zero-copy I/O | ✅ | ❌ |
| Hardware timestamping | ✅ | ❌ |
| PTP device access | ✅ | ❌ |
| Config validation (--no-net) | ✅ | ✅ |
| Self-test mode | ✅ | ✅ |
| Unit tests | ✅ | ✅ |
| Build system | ✅ | ✅ |

**Production deployment**: Linux only  
**Development**: Both Linux and macOS

---

## Contributing

### Code Style

- **C++17** standard
- **Strict warnings**: `-Wall -Wextra -Wpedantic -Werror`
- **Formatting**: clang-format (Google style)
- **Naming**:
  - Classes: `PascalCase`
  - Functions/methods: `camelCase`
  - Constants: `UPPER_SNAKE_CASE`
  - Member variables: `snake_case`

### Pull Request Process

1. **Fork** the repository
2. **Create** feature branch: `git checkout -b feature/my-feature`
3. **Write** tests for new functionality
4. **Build** with strict warnings: `cmake -DCMAKE_BUILD_TYPE=Release`
5. **Run** all tests: `cd build && ctest --output-on-failure`
6. **Test** with sanitizers: `cmake -DENABLE_ASAN=ON && cmake --build build && ./build/tests/vts_tests`
7. **Update** documentation if needed
8. **Submit** PR with clear description

### Development Workflow

```bash
# Build with sanitizers
cmake -S . -B build -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure

# Run specific test with verbose output
./tests/vts_tests --gtest_filter=BEREncodingTest.* --gtest_color=yes

# Check for memory leaks
valgrind --leak-check=full ./build/vts --selftest
```

---

## License

MIT License - see [LICENSE](LICENSE) file for details.

---

## References

### Standards

- **IEC 61850-8-1**: Communication networks and systems for power utility automation - Part 8-1: Specific communication service mapping (SCSM) - Mappings to MMS (ISO 9506-1 and ISO 9506-2) and to ISO/IEC 8802-3
- **IEC 61850-9-2**: Communication networks and systems for power utility automation - Part 9-2: Specific communication service mapping (SCSM) - Sampled values over ISO/IEC 8802-3
- **ITU-T X.690**: Information technology - ASN.1 encoding rules: Specification of Basic Encoding Rules (BER)
- **IEEE 802.1Q**: Virtual LANs (VLANs)
- **IEEE 1588**: Precision Time Protocol (PTP)

### Documentation

- [README-RT.md](README-RT.md) - Real-time Docker deployment guide
- [README-macos.md](README-macos.md) - macOS development guide
- [README_DOCKER.md](README_DOCKER.md) - Docker detailed reference (macvlan, SR-IOV)
- [tests/README.md](tests/README.md) - Unit test documentation
- [tests/TEST_SUMMARY.md](tests/TEST_SUMMARY.md) - Test coverage and results

### External Resources

- [Linux RT Wiki](https://wiki.linuxfoundation.org/realtime/start)
- [PREEMPT_RT Patches](https://kernel.org/pub/linux/kernel/projects/rt/)
- [Google Test Documentation](https://google.github.io/googletest/)
- [IEC 61850 Resources](https://www.iec61850.com/)

---

## Support

- **Issues**: [GitHub Issues](https://github.com/your-org/Virtual-TestSet/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/Virtual-TestSet/discussions)
- **Security**: Report vulnerabilities to security@your-org.com

---

**Last Updated**: Phase 14 - November 3, 2025
