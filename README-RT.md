# Real-Time Docker Deployment Guide

**VTS Production Deployment on Linux RT Hosts**

This guide covers deploying Virtual Test Set in a **real-time Docker container** on a Linux host with **PREEMPT_RT** kernel for deterministic microsecond-level performance.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [RT Kernel Installation](#rt-kernel-installation)
3. [Host System Tuning](#host-system-tuning)
4. [Docker Build & Run](#docker-build--run)
5. [Performance Verification](#performance-verification)
6. [Troubleshooting](#troubleshooting)
7. [Production Deployment](#production-deployment)

---

## Prerequisites

### Hardware Requirements

- **CPU**: x86_64 with 4+ cores (recommend isolating 2 cores for RT)
- **RAM**: 4GB minimum, 8GB recommended
- **Network**: Gigabit Ethernet with raw socket support
- **Optional**: Network card with hardware timestamping (e.g., Intel i350, i210)

### Software Requirements

- **Linux Distribution**: Ubuntu 22.04+, RHEL 8+, or Debian 11+
- **Kernel**: PREEMPT_RT patched kernel (see [RT Kernel Installation](#rt-kernel-installation))
- **Docker**: 20.10+ with cgroup v2 support
- **Docker Compose**: 2.0+

Check current setup:
```bash
# Check kernel RT config
zcat /proc/config.gz | grep PREEMPT
# Expected: CONFIG_PREEMPT_RT=y, CONFIG_PREEMPT_RT_FULL=y

# Check Docker version
docker --version
docker-compose --version

# Check cgroup version
grep cgroup /proc/filesystems
# v2 recommended for RT throttling control
```

---

## RT Kernel Installation

### Ubuntu

```bash
# Install RT kernel package
sudo apt-get update
sudo apt-get install linux-image-rt-amd64

# Set as default in GRUB
sudo update-grub

# Reboot
sudo reboot

# Verify RT kernel
uname -a
# Expected: "PREEMPT RT" in output
```

### Compile from Source (Advanced)

```bash
# Download kernel and RT patches
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.tar.xz
wget https://cdn.kernel.org/pub/linux/kernel/projects/rt/5.15/patch-5.15-rt.patch.xz

# Extract and apply RT patch
tar xf linux-5.15.tar.xz
cd linux-5.15
xzcat ../patch-5.15-rt.patch.xz | patch -p1

# Configure with RT options
make menuconfig
# Enable: General Setup → Preemption Model → Fully Preemptible Kernel (RT)
# Enable: Kernel Hacking → Kernel debugging → RT Debugging

# Build and install
make -j$(nproc)
sudo make modules_install install
sudo update-grub
sudo reboot
```

---

## Host System Tuning

### CPU Isolation

Isolate CPUs 2-3 for RT threads, leave CPUs 0-1 for kernel/IRQs:

```bash
# Edit GRUB configuration
sudo nano /etc/default/grub

# Add to GRUB_CMDLINE_LINUX:
GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"

# Update GRUB and reboot
sudo update-grub
sudo reboot

# Verify isolation
cat /sys/devices/system/cpu/isolated
# Expected: 2-3
```

**Explanation**:
- `isolcpus=2,3` - Remove CPUs 2-3 from general scheduler
- `nohz_full=2,3` - Reduce timer ticks on isolated CPUs
- `rcu_nocbs=2,3` - Move RCU callbacks off isolated CPUs

### CPU Governor

Set all CPUs to performance mode (disable power saving):

```bash
# Check current governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Set to performance
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Make permanent (Ubuntu)
sudo apt-get install cpufrequtils
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils
sudo systemctl restart cpufrequtils
```

### Network Interface Tuning

Disable offload features that introduce latency:

```bash
# Disable GRO/LRO/GSO/TSO
sudo ethtool -K eth0 gro off lro off gso off tso off

# Verify
sudo ethtool -k eth0 | grep -E '(gro|lro|gso|tso):'

# Make permanent (Ubuntu)
sudo nano /etc/network/interfaces
# Add: post-up ethtool -K $IFACE gro off lro off gso off tso off
```

Or use the provided script:
```bash
sudo ./scripts/disable_gro_lro.sh eth0
```

### IRQ Affinity

Pin network IRQs to non-RT CPUs (0-1):

```bash
# Use provided script
sudo ./scripts/pin_irqs.sh eth0 0,1

# Verify
grep eth0 /proc/interrupts
cat /proc/irq/*/smp_affinity_list
```

### Huge Pages (Optional)

Enable huge pages for reduced TLB misses:

```bash
# Configure 512 x 2MB huge pages
echo 512 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Make permanent
echo "vm.nr_hugepages=512" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### RT Limits

Configure RT scheduling limits:

```bash
# Edit limits.conf
sudo nano /etc/security/limits.conf

# Add:
* soft rtprio 95
* hard rtprio 95
* soft memlock unlimited
* hard memlock unlimited

# Logout and login for changes to take effect
```

### Verification Script

Run comprehensive RT environment check:

```bash
./scripts/verify_rt_env.sh

# Expected output: All checks PASS
# ✓ RT kernel detected
# ✓ CPU isolation configured
# ✓ Performance governor active
# ✓ Network offloads disabled
# ✓ IRQs pinned correctly
# ✓ RT limits configured
# ✓ Docker with RT capabilities
```

---

## Docker Build & Run

### Build Image

```bash
# Build multi-stage image
docker build -t vts:latest .

# Verify build
docker images | grep vts
```

### Run with Docker Compose (Recommended)

**docker-compose.yml** (included):
```yaml
version: '3.8'

services:
  vts:
    image: vts:latest
    container_name: vts
    network_mode: host  # Required for AF_PACKET raw sockets
    
    cap_add:
      - SYS_NICE       # SCHED_FIFO scheduling
      - NET_RAW        # Raw socket creation
      - NET_ADMIN      # Network interface configuration
      - IPC_LOCK       # mlockall memory locking
    
    ulimits:
      rtprio: 95       # RT priority limit
      memlock: -1      # Unlimited memory lock
    
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 1G
        reservations:
          cpus: '2.0'
          memory: 512M
    
    cpuset: "2,3"      # Pin to isolated CPUs
    
    environment:
      - IF_NAME=eth0
      - RT_PRIORITY=80
      - RT_CPU_AFFINITY=2,3
      - LOG_LEVEL=INFO
    
    volumes:
      - ./config:/app/config:ro
      - ./files:/app/files:rw
      - ./logs:/app/logs:rw
    
    devices:
      - /dev/ptp0:/dev/ptp0  # Optional: PTP hardware clock
    
    restart: unless-stopped
    
    healthcheck:
      test: ["CMD", "pgrep", "-f", "vts"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 10s
```

Start container:
```bash
docker-compose up -d

# View logs
docker-compose logs -f

# Check health
docker-compose ps

# Stop
docker-compose down
```

### Run with Docker CLI

```bash
docker run -d \
  --name vts \
  --network host \
  --cap-add=SYS_NICE \
  --cap-add=NET_RAW \
  --cap-add=NET_ADMIN \
  --cap-add=IPC_LOCK \
  --ulimit rtprio=95 \
  --ulimit memlock=-1 \
  --cpuset-cpus="2,3" \
  -e IF_NAME=eth0 \
  -e RT_PRIORITY=80 \
  -e RT_CPU_AFFINITY=2,3 \
  -v $(pwd)/config:/app/config:ro \
  -v $(pwd)/files:/app/files:rw \
  -v $(pwd)/logs:/app/logs:rw \
  --device /dev/ptp0:/dev/ptp0 \
  vts:latest
```

### cgroup v2 RT Throttling

Docker's default RT throttling may prevent RT thread creation. Disable it:

```bash
# Edit Docker daemon config
sudo nano /etc/docker/daemon.json

# Add:
{
  "cpu-rt-runtime": -1
}

# Restart Docker
sudo systemctl restart docker

# Verify
docker info | grep -i runtime
```

See [README_DOCKER.md](README_DOCKER.md#cgroup-v2-rt-throttling) for details.

---

## Performance Verification

### Check RT Scheduling

```bash
# Inside container
docker-compose exec vts bash

# Check thread priorities
ps -eLo pid,tid,class,rtprio,ni,pri,psr,comm | grep vts
# Expected: FF (FIFO) class, rtprio 80/90, psr showing CPU 2-3

# Check CPU affinity
taskset -cp $(pgrep vts)
# Expected: 2,3
```

### Cyclictest (Latency Testing)

Use the provided wrapper script:

```bash
# Run 10-minute latency test on isolated CPUs
sudo ./scripts/cyclictest_wrap.sh 2,3 600

# Expected results:
# Max latency < 50 µs (excellent)
# Max latency < 100 µs (good)
# Max latency > 200 µs (needs tuning)
```

Manual cyclictest:
```bash
sudo cyclictest -p 95 -a 2,3 -t2 -n -m -D 10m -h 200 -q
# -p 95: RT priority 95
# -a 2,3: Run on CPUs 2-3
# -t2: 2 threads
# -n: Use clock_nanosleep
# -m: Lock memory
# -D 10m: Duration 10 minutes
# -h 200: Histogram with 200 µs buckets
# -q: Quiet (only final stats)
```

### Packet Testing

Generate SV packets and measure timing:

```bash
# Inside container
docker-compose exec vts bash

# Enable debug logging
export LOG_LEVEL=DEBUG

# Run transient test (generates SV packets)
# Check logs for timing outliers
tail -f /app/logs/vts.log | grep TIMING
```

### Metrics Collection

```bash
# View metrics summary
docker-compose exec vts cat /app/logs/metrics.json

# Expected metrics:
{
  "uptime_seconds": 3600,
  "sentFrames": 34560000,  # 9600 Hz * 3600s
  "receivedFrames": 1234,
  "packetDrops": 0,        # Should be 0
  "parseErrors": 0,        # Should be 0
  "timingOutliers": 5,     # Low is good
  "maxTimingOutlier_us": 45  # < 100 µs is good
}
```

---

## Troubleshooting

### Container Won't Start

**Symptom**: Container exits immediately
```bash
docker-compose logs vts
```

**Check**:
1. **Capabilities**: Verify `cap_add` in docker-compose.yml
2. **RT Limits**: Check `/etc/security/limits.conf` and `/etc/docker/daemon.json`
3. **cgroup v2**: Disable RT throttling (see [cgroup v2 RT Throttling](#cgroup-v2-rt-throttling))
4. **Network interface**: Ensure `IF_NAME` matches actual interface

### High Latency

**Symptom**: Cyclictest shows max latency > 200 µs

**Tuning steps**:
1. **Verify RT kernel**: `uname -a` should show "PREEMPT RT"
2. **Check CPU isolation**: `cat /sys/devices/system/cpu/isolated` → 2-3
3. **Verify governor**: `cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` → performance
4. **Disable hyper-threading** in BIOS (optional but recommended)
5. **Check IRQ affinity**: `sudo ./scripts/pin_irqs.sh eth0 0,1`
6. **Disable unnecessary services**: `sudo systemctl disable <service>`

### Packet Drops

**Symptom**: Metrics show `packetDrops > 0`

**Solutions**:
1. **Increase RX ring buffer**: `sudo ethtool -G eth0 rx 4096`
2. **Check CPU load**: RT threads should be < 50% CPU
3. **Verify IRQ pinning**: IRQs on non-RT CPUs (0-1)
4. **Enable BPF filters**: Reduces kernel CPU load
5. **Check network load**: Use `iftop` or `nload`

### Permission Errors

**Symptom**: `Operation not permitted` errors

**Solutions**:
1. **Raw sockets**: Add `CAP_NET_RAW` capability
2. **RT scheduling**: Add `CAP_SYS_NICE` capability
3. **Memory locking**: Add `CAP_IPC_LOCK` capability
4. **RT limits**: Configure `/etc/security/limits.conf`

### No Hardware Timestamps

**Symptom**: Logs show "Hardware timestamping not available"

**Check NIC support**:
```bash
sudo ethtool -T eth0
# Expected: "hardware-receive" or "hardware-transmit" supported
```

**Fallback**: Software timestamps are used automatically (slightly lower precision)

---

## Production Deployment

### Systemd Service

Create `/etc/systemd/system/vts.service`:

```ini
[Unit]
Description=Virtual Test Set RT Container
After=docker.service
Requires=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
WorkingDirectory=/opt/vts
ExecStart=/usr/bin/docker-compose up -d
ExecStop=/usr/bin/docker-compose down
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable vts.service
sudo systemctl start vts.service
sudo systemctl status vts.service
```

### Health Monitoring

Use the built-in healthcheck:

```bash
# Check health status
docker inspect vts --format='{{json .State.Health.Status}}'
# Expected: "healthy"

# View health logs
docker inspect vts --format='{{range .State.Health.Log}}{{.Output}}{{end}}'
```

Integrate with monitoring (Prometheus/Grafana):
```bash
# Export metrics
curl http://localhost:8080/metrics > /var/log/vts_metrics.json

# Parse with jq
cat /var/log/vts_metrics.json | jq '.sentFrames'
```

### Log Rotation

Configure logrotate for VTS logs:

```bash
sudo nano /etc/logrotate.d/vts

# Add:
/opt/vts/logs/*.log {
    daily
    rotate 7
    compress
    delaycompress
    notifempty
    missingok
    create 0644 vts vts
}
```

### Backup Configuration

```bash
# Backup config directory
tar -czf vts_config_$(date +%Y%m%d).tar.gz /opt/vts/config

# Restore
tar -xzf vts_config_20231103.tar.gz -C /opt/vts/
```

---

## Network Mode Options

### Host Mode (Default)

**Advantages**:
- Full AF_PACKET raw socket support
- Lowest latency
- Hardware timestamping
- Promiscuous mode

**Disadvantages**:
- No network isolation
- Container shares host network stack

**Use for**: Production RT deployment

### Macvlan Mode

**Advantages**:
- L2 network isolation
- Own MAC address
- Multiple containers on same host

**Setup**:
```bash
# Create macvlan network
docker network create -d macvlan \
  --subnet=192.168.100.0/24 \
  --gateway=192.168.100.1 \
  --ip-range=192.168.100.128/25 \
  -o parent=eth0 \
  -o macvlan_mode=bridge \
  vts_macvlan

# Use docker-compose-macvlan.yml
docker-compose -f docker-compose-macvlan.yml up -d
```

See [README_DOCKER.md](README_DOCKER.md#macvlan-networking) for details.

### SR-IOV Mode

**Advantages**:
- Hardware isolation
- Dedicated VF per container
- Best performance
- DPDK compatibility (future)

**Requirements**:
- SR-IOV capable NIC (Intel X520, X710, Mellanox ConnectX)
- IOMMU enabled in BIOS
- VF creation and assignment

See [README_DOCKER.md](README_DOCKER.md#sr-iov-vf-passthrough) for setup guide.

---

## References

- [Linux RT Wiki](https://wiki.linuxfoundation.org/realtime/start)
- [PREEMPT_RT Documentation](https://wiki.linuxfoundation.org/realtime/documentation/start)
- [Docker Capabilities](https://docs.docker.com/engine/reference/run/#runtime-privilege-and-linux-capabilities)
- [cgroup v2 Documentation](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)
- [Network Interface Tuning](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/7/html/tuning_guide/index)

---

**Last Updated**: Phase 14 - November 3, 2025
