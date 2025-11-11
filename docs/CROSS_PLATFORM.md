# Cross-Platform Virtual TestSet Guide

## Overview

The Virtual TestSet has been designed to run on **Linux, Windows, and macOS** with adaptive real-time behavior. The system automatically detects the host platform and enables/disables features accordingly.

## Platform Support Matrix

| Feature | Linux | Windows | macOS |
|---------|-------|---------|-------|
| Raw Sockets (AF_PACKET) | ✅ Full | ❌ No | ❌ No |
| Real-Time Scheduling (SCHED_FIFO) | ✅ Full | ⚠️ Best-effort | ❌ No |
| Thread Priorities | ✅ SCHED_FIFO | ✅ SetThreadPriority | ⚠️ Limited |
| CPU Affinity | ✅ Full | ✅ Affinity Mask | ❌ No |
| Memory Locking | ✅ mlockall | ⚠️ Working Set | ❌ No |
| Network Operations | ✅ Full | ⚠️ --no-net | ⚠️ --no-net |
| Performance | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |

### Legend
- ✅ **Full**: Complete functionality with native APIs
- ⚠️ **Best-effort**: Available but not deterministic
- ❌ **No**: Not available (graceful fallback)

---

## Platform Behaviors

### Linux (Full Functionality)

**Capabilities:**
- **Raw Socket I/O**: Direct AF_PACKET access for GOOSE/SV transmission
- **SCHED_FIFO**: True real-time scheduling with priority 1-99
- **CPU Isolation**: `isolcpus`, `nohz_full`, `rcu_nocbs` kernel parameters
- **Memory Locking**: `mlockall()` prevents paging
- **IRQ Affinity**: Pin network interrupts to specific CPUs
- **TPACKET_V3**: Zero-copy packet I/O (kernel >= 3.2)

**Docker Requirements:**
```yaml
cap_add:
  - SYS_NICE       # SCHED_FIFO scheduling
  - NET_RAW        # Raw sockets
  - NET_ADMIN      # Interface config
  - IPC_LOCK       # mlockall

ulimits:
  rtprio: 95       # RT priority limit
  memlock: -1      # Unlimited memory lock

cpuset: "2,3"      # Pin to isolated CPUs
```

**Performance:**
- Packet latency: <10µs (with RT kernel)
- Timing jitter: <5µs (99th percentile)
- Throughput: Line-rate Gigabit Ethernet

---

### Windows (Best-Effort RT)

**Capabilities:**
- **Thread Priorities**: Maps Linux priority (1-99) to Windows classes:
  - Priority 90-99 → `REALTIME_PRIORITY_CLASS` + `THREAD_PRIORITY_TIME_CRITICAL`
  - Priority 70-89 → `HIGH_PRIORITY_CLASS` + `THREAD_PRIORITY_HIGHEST`
  - Priority 40-69 → `ABOVE_NORMAL_PRIORITY_CLASS`
  - Priority 1-39 → `NORMAL_PRIORITY_CLASS`
- **CPU Affinity**: `SetThreadAffinityMask()` for thread-to-core binding
- **Memory Locking**: `SetProcessWorkingSetSize()` (512 MB default)
- **Sleep Timing**: `QueryPerformanceCounter()` + spin-wait for sub-ms precision

**Limitations:**
- No raw sockets → **Requires `--no-net` mode**
- Cooperative scheduling (not preemptive RT)
- No deterministic latency guarantees
- Higher jitter than Linux

**Docker on Windows:**
```dockerfile
# Windows Server Core base image
FROM mcr.microsoft.com/windows/servercore:ltsc2022 AS build

# Install Visual Studio Build Tools (MSVC)
# ... build steps ...

FROM mcr.microsoft.com/windows/nanoserver:ltsc2022
# ... runtime configuration ...
```

**Performance:**
- Packet latency: N/A (no-net mode)
- Timing jitter: ~1ms (best case)
- Throughput: N/A

**Use Case:**
- Development/testing on Windows workstations
- CI/CD pipelines on Windows runners
- Non-network operations (e.g., COMTRADE file analysis)

---

### macOS (No-Net Mode)

**Capabilities:**
- **BPF Access**: Available but not used (requires root)
- **Thread Priorities**: Limited Mach APIs (not implemented)
- **No RT Guarantees**: No equivalent to SCHED_FIFO

**Limitations:**
- No raw sockets → **Requires `--no-net` mode**
- No real-time scheduling
- No CPU affinity control
- No memory locking

**Docker on macOS:**
```bash
# Docker Desktop for Mac (uses VM internally)
docker run --rm -it \
  -v $(pwd):/app \
  -e NO_NET=true \
  vts:latest
```

**Performance:**
- Packet latency: N/A (no-net mode)
- Timing jitter: ~10ms (macOS VM overhead)
- Throughput: N/A

**Use Case:**
- Local development on macOS laptops
- Unit testing (no network I/O)
- Configuration file validation

---

## Build System

### CMakeLists.txt Platform Detection

```cmake
# Automatic platform detection
if(UNIX AND NOT APPLE)
    # Linux
    set(VTS_PLATFORM "LINUX")
    find_library(PTHREAD_LIBRARY pthread REQUIRED)
    target_link_libraries(vts PRIVATE pthread)
    
elseif(WIN32)
    # Windows
    set(VTS_PLATFORM "WINDOWS")
    target_link_libraries(vts PRIVATE ws2_32)  # Winsock
    
elseif(APPLE)
    # macOS
    set(VTS_PLATFORM "MAC")
    find_library(PTHREAD_LIBRARY pthread REQUIRED)
    target_link_libraries(vts PRIVATE pthread)
endif()

add_definitions(-DVTS_PLATFORM_${VTS_PLATFORM})
```

### Platform Headers

- **`compat.hpp`**: Platform detection macros
  ```cpp
  #ifdef VTS_PLATFORM_LINUX
    // Linux code
  #elif defined(VTS_PLATFORM_WINDOWS)
    // Windows code
  #elif defined(VTS_PLATFORM_MAC)
    // macOS code
  #endif
  ```

- **`rt_utils.hpp/cpp`**: Unified RT API
  - `rt_set_realtime(int priority)`: Set thread priority
  - `rt_set_affinity(std::vector<int>)`: Bind thread to CPUs
  - `rt_lock_memory()`: Lock memory pages
  - `rt_sleep_abs(uint64_t)`: Absolute-time sleep

---

## Docker Multi-Platform Setup

### Linux Host (Optimal)

```bash
# Build for Linux
docker build -f docker/Dockerfile.backend -t vts:linux .

# Run with full RT capabilities
docker-compose -f backend/docker-compose.yml up
```

### Windows Host

```bash
# Build for Windows
docker build -f docker/Dockerfile.backend.windows -t vts:windows .

# Run in no-net mode
docker run --rm -it \
  -v ${PWD}/backend/files:/app/files \
  -e NO_NET=true \
  vts:windows
```

### macOS Host (via Docker Desktop)

```bash
# Build Linux image (runs in Docker VM)
docker build -f docker/Dockerfile.backend -t vts:macos .

# Run in no-net mode (or with limited network)
docker run --rm -it \
  -v $(pwd)/backend/files:/app/files \
  -e NO_NET=true \
  vts:macos
```

---

## Runtime Detection

The application logs platform capabilities at startup:

```
[RT] === Platform Detection ===
[RT] Platform: Linux (full functionality: raw sockets, RT, TPACKET_V3)
[RT] Raw sockets supported: YES
[RT] Linux RT operations supported: YES
[RT] Thread priority control: YES
[RT] === Linux RT Initialization ===
[RT] Memory locked (mlockall MCL_CURRENT | MCL_FUTURE)
[RT] Linux real-time initialization complete
[RT] =================================
```

**Windows Example:**
```
[RT] === Platform Detection ===
[RT] Platform: Windows (limited: no raw sockets, best-effort RT via thread priorities)
[RT] Raw sockets supported: NO
[RT] Linux RT operations supported: NO
[RT] Thread priority control: YES
[RT] === Windows Best-Effort RT Initialization ===
[RT] Windows detected - using thread priorities instead of SCHED_FIFO
[RT] Performance note: Windows thread scheduling is cooperative, not deterministic
[RT] Memory working set locked on Windows (best-effort, 512 MB)
[RT] Windows initialization complete
[RT] =================================
```

---

## Performance Tuning Per Platform

### Linux: Maximum Performance

1. **Use RT-patched kernel**:
   ```bash
   uname -v | grep PREEMPT_RT
   ```

2. **Isolate CPUs**:
   ```bash
   # /etc/default/grub
   GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
   sudo update-grub && reboot
   ```

3. **Pin IRQs**:
   ```bash
   sudo ./backend/scripts/pin_irqs.sh eth0 0,1
   ```

4. **Set CPU governor**:
   ```bash
   sudo cpupower frequency-set -g performance
   ```

### Windows: Best-Effort Optimization

1. **Disable power saving**:
   ```powershell
   powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
   ```

2. **Set process priority** (in code):
   ```cpp
   SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
   ```

3. **Disable background services**:
   - Windows Defender (during tests)
   - Windows Update
   - Superfetch

### macOS: Graceful Degradation

1. **Use `--no-net` mode**:
   ```bash
   ./vts --no-net
   ```

2. **Accept higher jitter** (10-50ms typical)

3. **Focus on correctness, not timing**

---

## Docker Compose Examples

### Linux Production Deployment

```yaml
# docker-compose.yml
version: '3.8'
services:
  vts:
    image: vts:linux
    network_mode: host
    cap_add:
      - SYS_NICE
      - NET_RAW
      - NET_ADMIN
      - IPC_LOCK
    ulimits:
      rtprio: 95
      memlock: -1
    cpuset: "2,3"
    environment:
      - IF_NAME=eth0
      - RT_PRIORITY=80
```

### Windows Testing Deployment

```yaml
# docker-compose.windows.yml
version: '3.8'
services:
  vts:
    image: vts:windows
    environment:
      - NO_NET=true
    volumes:
      - ./backend/files:/app/files
```

### macOS Development Deployment

```yaml
# docker-compose.macos.yml
version: '3.8'
services:
  vts:
    image: vts:linux  # Runs in Docker Desktop VM
    environment:
      - NO_NET=true
    ports:
      - "8080:8080"
      - "8090:8090"
    volumes:
      - ./backend/files:/app/files
```

---

## API Compatibility

All RT functions are safe to call on any platform:

```cpp
// This code works on Linux, Windows, and macOS
rt_set_realtime(80);       // Sets priority (or logs warning)
rt_set_affinity({2, 3});   // Sets affinity (or no-op)
rt_lock_memory();          // Locks memory (or no-op)
```

**Return Values:**
- **Linux**: `true` on success, `false` + log on failure
- **Windows**: `true` on success, `false` + log on failure
- **macOS**: Always `false` with INFO log

---

## Troubleshooting

### Linux: "SCHED_FIFO failed: Operation not permitted"

**Cause**: Missing `CAP_SYS_NICE` capability

**Fix**:
```yaml
cap_add:
  - SYS_NICE
ulimits:
  rtprio: 95
```

### Windows: "SetThreadPriority failed"

**Cause**: Insufficient process privileges

**Fix**: Run Docker with admin rights or adjust process security policy

### macOS: "Network operations not supported"

**Expected**: macOS requires `--no-net` mode

**Fix**: Add `NO_NET=true` environment variable

---

## Summary

| Platform | Best Use Case | Performance |
|----------|---------------|-------------|
| **Linux** | Production RT servers | ⭐⭐⭐⭐⭐ |
| **Windows** | Development/Testing | ⭐⭐⭐ |
| **macOS** | Local development | ⭐⭐ |

**Recommendation**: Use **Linux** for production deployments with strict timing requirements. Use **Windows/macOS** for development, CI/CD, and non-network testing.

---

## References

- `backend/src/platform/include/compat.hpp`: Platform detection
- `backend/src/tools/src/rt_utils.cpp`: RT abstraction layer
- `backend/README-RT.md`: Linux RT tuning guide
- `backend/README-macos.md`: macOS development guide
