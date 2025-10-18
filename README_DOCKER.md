# Docker Deployment Guide - Virtual TestSet

## Overview

This document describes how to build and run Virtual TestSet in a Docker container with real-time capabilities. The container is configured for low-latency IEC 61850 GOOSE/SV packet processing with proper CPU isolation, memory locking, and network access.

---

## Prerequisites

### 1. System Requirements

- **OS**: Linux (Ubuntu 20.04+, Debian 11+, or RHEL 8+)
- **Kernel**: Recommended PREEMPT_RT kernel for hard real-time
  - Check: `uname -v | grep PREEMPT`
- **CPU**: Multi-core (≥4 cores recommended for isolation)
- **RAM**: ≥4 GB
- **Network**: Physical Ethernet interface for GOOSE/SV

### 2. Software Requirements

- Docker 20.10+
- Docker Compose 1.29+ (or Docker with compose plugin)
- Root/sudo access for capabilities and host tuning

### 3. Verify Environment

Run the verification script to check your system:

```bash
sudo ./scripts/verify_rt_env.sh
```

This checks:

- Kernel preemption support
- CPU isolation
- Network configuration
- Docker installation
- Real-time capabilities

---

## Host System Tuning (Recommended)

For optimal real-time performance, tune the host system before running the container:

### 1. Disable Network Offloading

GRO/LRO can introduce latency and jitter:

```bash
sudo ./scripts/disable_gro_lro.sh eth0
```

Replace `eth0` with your network interface name.

### 2. Pin IRQs to Specific CPUs

Isolate interrupt handling from real-time application CPUs:

```bash
# Pin IRQs to CPUs 0-1, leave 2-3 for RT app
sudo ./scripts/pin_irqs.sh eth0 0,1
```

### 3. Isolate CPUs (Persistent)

Add to kernel boot parameters in `/etc/default/grub`:

```
GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
```

Then update GRUB and reboot:

```bash
sudo update-grub
sudo reboot
```

### 4. Set CPU Governor to Performance

```bash
sudo cpupower frequency-set -g performance
```

### 5. Configure Memory

```bash
# Reduce swappiness
sudo sysctl vm.swappiness=1

# Allocate huge pages
echo 128 | sudo tee /proc/sys/vm/nr_hugepages
```

---

## Build and Run

### Quick Start

```bash
# Build the image
docker compose build

# Run with default settings
docker compose up -d

# View logs
docker compose logs -f

# Stop
docker compose down
```

### Build Options

**Development build** (with debug symbols):

```bash
docker compose build --build-arg BUILD_TYPE=Debug
```

**Production build** (optimized):

```bash
docker compose build --build-arg BUILD_TYPE=Release
```

### Configuration

Edit `docker-compose.yml` to customize:

```yaml
environment:
  IF_NAME: eth0              # Network interface
  RT_PRIORITY: 80            # RT priority (1-99)
  RT_CPU_AFFINITY: "2,3"     # CPU cores for RT threads
  TZ: America/Sao_Paulo      # Timezone
```

---

## CPU Isolation and Pinning

### Understanding CPU Assignment

- **CPUs 0-1**: Handle IRQs, OS tasks, non-RT work
- **CPUs 2-3**: Isolated for real-time application

### Configure CPU Pinning

**Method 1: Docker Compose** (for development):

```yaml
deploy:
  resources:
    limits:
      cpus: '2'
      memory: 2G
    reservations:
      cpus: '2'
      memory: 1G
```

**Method 2: CLI** (for production, exact pinning):

```bash
docker run -d \
  --name virtual-testset \
  --cpuset-cpus=2,3 \
  --cpu-rt-runtime=950000 \
  --ulimit rtprio=95 \
  --ulimit memlock=-1 \
  --cap-add=SYS_NICE \
  --cap-add=NET_RAW \
  --cap-add=NET_ADMIN \
  --cap-add=IPC_LOCK \
  --network=host \
  -v ./config:/app/config:ro \
  -v ./files:/app/files:rw \
  -e IF_NAME=eth0 \
  -e RT_PRIORITY=80 \
  -e RT_CPU_AFFINITY="2,3" \
  virtual-testset:latest
```

---

## Network Configuration

### Host Networking Mode

The container uses `network_mode: host` to access physical interfaces directly. This is required for:

- AF_PACKET raw sockets
- Promiscuous mode
- VLAN tagging
- Sub-microsecond timestamps

**Security note**: Host networking bypasses Docker's network isolation. Only run trusted containers with this mode.

### Interface Selection

Set the interface in `docker-compose.yml`:

```yaml
environment:
  IF_NAME: eth0  # Change to your interface
```

List available interfaces:

```bash
ip -br link show
```

---

## Volumes and Persistence

### Volume Mapping

```yaml
volumes:
  - ./config:/app/config:ro      # Configuration files (read-only)
  - ./files:/app/files:rw        # Input/output files (read-write)
  - ./logs:/app/logs:rw          # Application logs (read-write)
```

### Configuration Files

Place JSON configs in `./config/`:

```bash
./config/
├── goose_config.json
├── sv_config.json
└── test_config.json
```

Example `goose_config.json`:

```json
{
  "interface": "eth0",
  "goAppId": 1,
  "macAddress": "01:0C:CD:01:00:01",
  "vlanId": 100,
  "vlanPriority": 4
}
```

---

## Real-Time Capabilities

### Required Capabilities

The container needs these Linux capabilities:

- **CAP_SYS_NICE**: Set RT scheduling (SCHED_FIFO)
- **CAP_NET_RAW**: Create AF_PACKET sockets
- **CAP_NET_ADMIN**: Set interface to promiscuous mode
- **CAP_IPC_LOCK**: Lock memory (mlockall)

### Resource Limits (ulimits)

```yaml
ulimits:
  rtprio:
    soft: 95
    hard: 95
  memlock:
    soft: -1
    hard: -1
```

- `rtprio`: Maximum RT priority (1-99)
- `memlock`: Unlimited memory locking

---

## Testing and Validation

### 1. Verify Container is Running

```bash
docker compose ps
docker compose logs --tail 50
```

Expected output:
```
virtual-testset  | [INFO] Interface: eth0
virtual-testset  | [INFO] RT Priority: 80
virtual-testset  | [INFO] CPU Affinity: 2,3
virtual-testset  | [INFO] Memory locked: 1234 KB
virtual-testset  | [INFO] GOOSE receiver started on VLAN 100
```

### 2. Check Resource Usage

```bash
docker stats virtual-testset
```

### 3. Inspect RT Configuration

```bash
# Check scheduling policy (should be SCHED_FIFO)
docker exec virtual-testset chrt -p 1

# Check CPU affinity
docker exec virtual-testset taskset -cp 1

# Check memory locking
docker exec virtual-testset grep VmLck /proc/1/status
```

### 4. Test Latency (cyclictest)

```bash
sudo ./scripts/cyclictest_wrap.sh 60 80 2,3
```

Acceptable latencies:
- **<100 µs**: Excellent
- **<200 µs**: Good
- **<500 µs**: Acceptable
- **>500 µs**: Poor (investigate)

### 5. Send Test Packets

```bash
# From host
docker exec virtual-testset /app/virtual_testset --mode tx --type goose

# Monitor with tcpdump
sudo tcpdump -i eth0 -nn ether proto 0x88b8
```

---

## Troubleshooting

### Container Fails to Start

**Error**: `Failed to set scheduling policy`

**Cause**: Missing `CAP_SYS_NICE` or insufficient `rtprio` limit

**Fix**:
```bash
# Check capabilities
docker inspect virtual-testset | grep CapAdd

# Verify ulimits
docker exec virtual-testset ulimit -r
```

---

### Memory Lock Fails

**Error**: `mlockall failed: Cannot allocate memory`

**Cause**: `memlock` limit too low

**Fix**:
```yaml
ulimits:
  memlock:
    soft: -1  # Unlimited
    hard: -1
```

---

### Cannot Open Network Interface

**Error**: `socket(AF_PACKET) failed: Operation not permitted`

**Cause**: Missing `CAP_NET_RAW`

**Fix**:
```yaml
cap_add:
  - NET_RAW
  - NET_ADMIN
```

---

### High Latency / Jitter

**Symptoms**: Packet delays >1ms, missed deadlines

**Diagnosis**:
```bash
# Run latency test
sudo ./scripts/cyclictest_wrap.sh 60 80 2,3

# Check IRQ affinity
cat /proc/interrupts | grep eth0
```

**Fixes**:
1. Disable GRO/LRO: `sudo ./scripts/disable_gro_lro.sh eth0`
2. Pin IRQs: `sudo ./scripts/pin_irqs.sh eth0 0,1`
3. Use PREEMPT_RT kernel
4. Increase RT priority: `RT_PRIORITY=90`
5. Verify CPU isolation: `cat /sys/devices/system/cpu/isolated`

---

### Permission Denied Errors

**Error**: `permission denied` when accessing `/app/files`

**Cause**: Volume permissions mismatch

**Fix**:
```bash
# Set ownership to container user (UID 1000)
sudo chown -R 1000:1000 ./files ./logs

# Or run as root (not recommended)
user: "0:0"
```

---

### VLAN Packets Not Received

**Symptoms**: Container runs but no GOOSE/SV packets detected

**Diagnosis**:
```bash
# Check VLAN interface exists
docker exec virtual-testset ip link show

# Monitor raw traffic
docker exec virtual-testset tcpdump -i eth0 -nn -e vlan
```

**Fixes**:
1. Verify VLAN config in JSON
2. Check switch port is trunk mode
3. Ensure host interface is up: `ip link set eth0 up`
4. Test without Docker first: `tcpdump -i eth0 vlan 100`

---

## Performance Tuning

### Recommended Settings

For 4-core system with IEC 61850 workload:

```yaml
# docker-compose.yml
environment:
  IF_NAME: eth0
  RT_PRIORITY: 85
  RT_CPU_AFFINITY: "2,3"

deploy:
  resources:
    limits:
      cpus: '2'
      memory: 2G
```

```bash
# Host tuning
sudo ./scripts/disable_gro_lro.sh eth0
sudo ./scripts/pin_irqs.sh eth0 0,1

# Kernel parameters (persistent)
# /etc/default/grub:
GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
```

### Latency Targets

| Protocol | Max Latency | Typical |
|----------|-------------|---------|
| GOOSE    | 4 ms        | <1 ms   |
| SV       | 4 ms        | <1 ms   |
| API      | 100 ms      | <10 ms  |

---

## Security Considerations

### Capabilities

The container requires elevated privileges for real-time operation. Mitigations:

1. **Run as non-root user** (UID 1000, `vts:vts`)
2. **Read-only config volume**
3. **Drop unused capabilities**:
   ```yaml
   cap_drop:
     - ALL
   cap_add:
     - SYS_NICE
     - NET_RAW
     - NET_ADMIN
     - IPC_LOCK
   ```

### Network Isolation

Host networking is required for AF_PACKET. To minimize risk:

1. Run on isolated OT network segment
2. Use firewall rules to restrict container
3. Monitor with IDS (Suricata, Snort)

---

## Production Deployment

### Systemd Service

Create `/etc/systemd/system/virtual-testset.service`:

```ini
[Unit]
Description=Virtual TestSet IEC 61850 Container
After=docker.service network-online.target
Requires=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=/opt/virtual-testset
ExecStartPre=/usr/local/bin/docker compose pull
ExecStart=/usr/local/bin/docker compose up -d
ExecStop=/usr/local/bin/docker compose down
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable virtual-testset
sudo systemctl start virtual-testset
```

### Health Monitoring

The container includes a healthcheck:

```dockerfile
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD pgrep -f virtual_testset || exit 1
```

Check status:

```bash
docker inspect --format='{{.State.Health.Status}}' virtual-testset
```

---

## References

- [Docker Runtime Constraints](https://docs.docker.com/config/containers/resource_constraints/)
- [Linux Real-Time Tuning Guide](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/8/)
- [PREEMPT_RT Kernel](https://wiki.linuxfoundation.org/realtime/start)
- [IEC 61850-90-5](https://webstore.iec.ch/publication/6028)

---

## Support

For issues, see:
- `AGENT_PROGRESS_VTS.txt` - Development log
- `improvements.md` - Roadmap and known issues
- GitHub Issues (if applicable)

---

**Last Updated**: Phase 9 - Docker for Real-Time
