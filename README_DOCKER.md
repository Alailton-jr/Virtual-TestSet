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

## NIC Attachment Options

Virtual TestSet supports multiple network attachment strategies for different deployment scenarios. This section covers host networking (default), macvlan, and SR-IOV VF options.

### Overview of Network Modes

| Mode | Isolation | Performance | Complexity | Use Case |
|------|-----------|-------------|------------|----------|
| **Host** | None | Best | Low | Single container, direct hardware access |
| **Macvlan** | L2 | Good | Medium | Multiple containers, separate MAC addresses |
| **SR-IOV VF** | Hardware | Best | High | Multiple containers, dedicated NIC resources |

---

### Option 1: Host Networking (Default)

**Description:** Container shares the host's network namespace. Direct access to all host interfaces.

**Advantages:**
- ✅ Simplest configuration
- ✅ Best performance (no virtualization overhead)
- ✅ Direct AF_PACKET access
- ✅ Hardware timestamping works transparently
- ✅ PTP devices accessible

**Disadvantages:**
- ⚠️ No network isolation (security risk)
- ⚠️ Port conflicts between containers
- ⚠️ All containers see same interfaces

**Configuration:**

```yaml
# docker-compose.yml
services:
  virtual-testset:
    network_mode: host
    cap_add:
      - NET_RAW
      - NET_ADMIN
    environment:
      - IF_NAME=eth0
```

**Usage:**

```bash
# Start container
docker compose up -d

# Container sees host interfaces
docker exec virtual-testset ip link show
```

**Security Considerations:**
- Run on isolated OT network segment
- Use firewall rules to restrict access
- Non-root user inside container (vts:vts)

---

### Option 2: Macvlan Network

**Description:** Creates virtual network interfaces with separate MAC addresses. Each container appears as a distinct device on the network.

**Advantages:**
- ✅ L2 network isolation
- ✅ Separate MAC addresses per container
- ✅ Multiple containers on same host interface
- ✅ Better security than host mode

**Disadvantages:**
- ⚠️ Host cannot directly communicate with containers
- ⚠️ ARP/NDP traffic may need special handling
- ⚠️ Some NICs limit number of MAC addresses
- ⚠️ Hardware timestamping may not work

**Setup Steps:**

#### 1. Create Macvlan Network

```bash
# Create macvlan network attached to physical interface
docker network create -d macvlan \
  --subnet=192.168.100.0/24 \
  --gateway=192.168.100.1 \
  --ip-range=192.168.100.128/25 \
  -o parent=eth0 \
  -o macvlan_mode=bridge \
  vts_macvlan
```

**Parameters:**
- `--subnet`: Network subnet (must match your network)
- `--gateway`: Network gateway
- `--ip-range`: IP range for containers (avoid DHCP range)
- `-o parent=eth0`: Physical interface to attach to
- `macvlan_mode=bridge`: Bridge mode (containers can talk to each other)

**Macvlan Modes:**

| Mode | Description | Inter-Container | External |
|------|-------------|-----------------|----------|
| `bridge` | Containers can communicate | ✅ Yes | ✅ Yes |
| `vepa` | VEPA (hairpin) mode | Via switch | ✅ Yes |
| `private` | No inter-container traffic | ❌ No | ✅ Yes |
| `passthru` | Single container, exclusive | N/A | ✅ Yes |

#### 2. Docker Compose Configuration

```yaml
# docker-compose-macvlan.yml
version: '3.8'

services:
  virtual-testset-1:
    build: .
    container_name: vts-1
    
    networks:
      vts_macvlan:
        ipv4_address: 192.168.100.130
    
    cap_add:
      - SYS_NICE
      - NET_RAW
      - NET_ADMIN
      - IPC_LOCK
    
    ulimits:
      rtprio: 95
      memlock: -1
    
    environment:
      - IF_NAME=eth0      # Interface inside container
      - RT_PRIORITY=80
    
    volumes:
      - ./config:/app/config:ro
      - ./files:/app/files:rw
      - ./logs/vts1:/app/logs:rw

  virtual-testset-2:
    build: .
    container_name: vts-2
    
    networks:
      vts_macvlan:
        ipv4_address: 192.168.100.131
    
    cap_add:
      - SYS_NICE
      - NET_RAW
      - NET_ADMIN
      - IPC_LOCK
    
    ulimits:
      rtprio: 95
      memlock: -1
    
    environment:
      - IF_NAME=eth0
      - RT_PRIORITY=80
    
    volumes:
      - ./config:/app/config:ro
      - ./files:/app/files:rw
      - ./logs/vts2:/app/logs:rw

networks:
  vts_macvlan:
    external: true
```

#### 3. Start Containers

```bash
# Start multiple containers
docker compose -f docker-compose-macvlan.yml up -d

# Verify network attachment
docker exec vts-1 ip addr show eth0
docker exec vts-2 ip addr show eth0
```

**Troubleshooting Macvlan:**

**Problem: Host cannot ping containers**

**Cause:** Linux kernel doesn't route between macvlan interface and parent

**Solution:** Create macvlan interface on host for communication

```bash
# Create macvlan interface on host
sudo ip link add mac-host link eth0 type macvlan mode bridge
sudo ip addr add 192.168.100.254/24 dev mac-host
sudo ip link set mac-host up

# Now host can communicate with containers
ping 192.168.100.130
```

**Problem: ARP not working**

**Cause:** Switch may filter ARP based on port security

**Solution:** 
1. Disable port security on switch port
2. Use `vepa` mode (requires switch support)
3. Use `bridge` mode with switch learning enabled

**Problem: Hardware timestamps not available**

**Cause:** Macvlan doesn't support PTP

**Workaround:** Use software timestamps or host networking

---

### Option 3: SR-IOV Virtual Functions (VF)

**Description:** Hardware-accelerated network virtualization. Each container gets a dedicated Virtual Function (VF) from the physical NIC.

**Advantages:**
- ✅ Hardware isolation
- ✅ Dedicated bandwidth per container
- ✅ Near-native performance
- ✅ No overhead from bridge/NAT
- ✅ Hardware timestamping supported
- ✅ VLAN and QoS hardware offload

**Disadvantages:**
- ⚠️ Requires SR-IOV capable NIC
- ⚠️ Complex setup
- ⚠️ Limited number of VFs per NIC (typically 32-128)
- ⚠️ May require BIOS/UEFI configuration

**Requirements:**
- SR-IOV capable NIC (Intel I350/X540/X710, Mellanox ConnectX series)
- IOMMU enabled in BIOS
- Linux kernel with SR-IOV support

#### 1. Enable SR-IOV

**Check NIC Capability:**

```bash
# Check if NIC supports SR-IOV
lspci -v | grep -i "Single Root I/O"

# Check current SR-IOV status
lspci -s 01:00.0 -vvv | grep SR-IOV
```

**Enable IOMMU:**

Add to `/etc/default/grub`:

```
GRUB_CMDLINE_LINUX="intel_iommu=on iommu=pt"
```

Update GRUB and reboot:

```bash
sudo update-grub
sudo reboot
```

**Create Virtual Functions:**

```bash
# Find PCI address of NIC
lspci | grep Ethernet
# Example output: 01:00.0 Ethernet controller: Intel Corporation I350

# Enable 4 VFs on Intel I350
echo 4 | sudo tee /sys/class/net/eth0/device/sriov_numvfs

# Verify VFs created
ip link show | grep vf
lspci | grep "Virtual Function"
```

**Make Persistent:**

Create `/etc/systemd/system/sriov-vf.service`:

```ini
[Unit]
Description=Enable SR-IOV Virtual Functions
After=network.target

[Service]
Type=oneshot
ExecStart=/bin/bash -c 'echo 4 > /sys/class/net/eth0/device/sriov_numvfs'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Enable service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable sriov-vf
sudo systemctl start sriov-vf
```

#### 2. Configure Virtual Functions

**Set MAC Address, VLAN, and Rate Limit:**

```bash
# VF 0: MAC, VLAN 100, rate 1000 Mbps
sudo ip link set eth0 vf 0 mac 00:11:22:33:44:50 vlan 100 rate 1000

# VF 1: MAC, VLAN 200, rate 1000 Mbps
sudo ip link set eth0 vf 1 mac 00:11:22:33:44:51 vlan 200 rate 1000

# Verify configuration
ip link show eth0
```

**Example Output:**

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP mode DEFAULT group default qlen 1000
    link/ether a0:36:9f:12:34:56 brd ff:ff:ff:ff:ff:ff
    vf 0 MAC 00:11:22:33:44:50, vlan 100, spoof checking on, link-state auto, trust off, query_rss off
    vf 1 MAC 00:11:22:33:44:51, vlan 200, spoof checking on, link-state auto, trust off, query_rss off
```

#### 3. Assign VF to Container (Kernel Stack Path)

**Method A: Using Pipework (Simple)**

```bash
# Install pipework
git clone https://github.com/jpetazzo/pipework
cd pipework && sudo cp pipework /usr/local/bin/

# Start container without network
docker run -d --name vts-1 \
  --network none \
  --cap-add=SYS_NICE \
  --cap-add=NET_RAW \
  --cap-add=NET_ADMIN \
  --cap-add=IPC_LOCK \
  --ulimit rtprio=95 \
  --ulimit memlock=-1 \
  virtual-testset:latest

# Move VF into container namespace
sudo pipework eth0 -i eth0 vts-1 192.168.100.130/24 @00:11:22:33:44:50

# Verify inside container
docker exec vts-1 ip link show eth0
```

**Method B: Using IP Netns (Manual)**

```bash
# Get VF interface name
VF_IFACE=$(ls /sys/class/net/eth0/device/virtfn0/net/)

# Start container
docker run -d --name vts-1 \
  --network none \
  --cap-add=SYS_NICE \
  --cap-add=NET_RAW \
  --cap-add=NET_ADMIN \
  --cap-add=IPC_LOCK \
  --ulimit rtprio=95 \
  --ulimit memlock=-1 \
  virtual-testset:latest

# Get container PID
CPID=$(docker inspect -f '{{.State.Pid}}' vts-1)

# Create netns link
sudo ln -s /proc/$CPID/ns/net /var/run/netns/$CPID

# Move VF to container
sudo ip link set $VF_IFACE netns $CPID name eth0

# Configure inside container
sudo ip netns exec $CPID ip addr add 192.168.100.130/24 dev eth0
sudo ip netns exec $CPID ip link set eth0 up
```

**Method C: Docker Compose with SR-IOV Plugin**

```yaml
# docker-compose-sriov.yml
version: '3.8'

services:
  virtual-testset-1:
    build: .
    container_name: vts-sriov-1
    
    networks:
      sriov_net:
        ipv4_address: 192.168.100.130
    
    cap_add:
      - SYS_NICE
      - NET_RAW
      - NET_ADMIN
      - IPC_LOCK
    
    ulimits:
      rtprio: 95
      memlock: -1
    
    deploy:
      resources:
        limits:
          cpus: '2'
        reservations:
          cpus: '2'
    
    environment:
      - IF_NAME=net0
      - RT_PRIORITY=80
    
    volumes:
      - ./config:/app/config:ro
      - ./files:/app/files:rw
      - ./logs:/app/logs:rw

networks:
  sriov_net:
    driver: sriov
    driver_opts:
      resourceName: "intel.com/sriov_netdevice"
    ipam:
      config:
        - subnet: 192.168.100.0/24
          gateway: 192.168.100.1
```

#### 4. SR-IOV Performance Tuning

**Disable Spoof Checking (if needed):**

```bash
sudo ip link set eth0 vf 0 spoofchk off
```

**Enable Trust Mode (allows promisc/multicast):**

```bash
sudo ip link set eth0 vf 0 trust on
```

**Set Link State:**

```bash
# Force link up
sudo ip link set eth0 vf 0 state enable

# Auto (default)
sudo ip link set eth0 vf 0 state auto
```

#### 5. Future: DPDK Path (Advanced)

For ultra-low latency (<1 µs), SR-IOV VFs can be used with DPDK (Data Plane Development Kit):

**DPDK Benefits:**
- Kernel bypass (no system calls)
- Poll-mode drivers (no interrupts)
- Zero-copy packet processing
- <1 µs latency achievable

**Setup Overview:**

```bash
# Bind VF to DPDK driver (vfio-pci or igb_uio)
sudo dpdk-devbind.py --bind=vfio-pci 0000:01:10.0

# Pass VF to container with --device
docker run --device=/dev/vfio/vfio --device=/dev/vfio/X ...
```

**Note:** DPDK integration is not implemented in current version. See `improvements.md` for future roadmap.

---

### PTP Device Exposure

For hardware timestamping, PTP devices need to be accessible inside containers.

**Check PTP Devices:**

```bash
# List PTP devices
ls -l /dev/ptp*

# Example output:
# crw------- 1 root root 248, 0 Oct 18 10:00 /dev/ptp0
# crw------- 1 root root 248, 1 Oct 18 10:00 /dev/ptp1
```

**Option 1: Host Networking**

PTP devices automatically visible (no config needed).

**Option 2: Device Passthrough**

```yaml
# docker-compose.yml
services:
  virtual-testset:
    devices:
      - /dev/ptp0:/dev/ptp0
```

**Option 3: Privileged Mode (Not Recommended)**

```yaml
services:
  virtual-testset:
    privileged: true  # ⚠️ Security risk
```

**Test PTP Access:**

```bash
# Inside container
docker exec virtual-testset ls -l /dev/ptp0

# Test PTP clock read
docker exec virtual-testset cat /sys/class/ptp/ptp0/clock_name
```

---

### Cgroup v2 RT Throttling

**Issue:** On systems with cgroup v2, RT threads may be throttled even with correct ulimits and capabilities.

**Symptoms:**
- `sched_setscheduler` fails with `EPERM`
- RT priority cannot be set
- Error: "Operation not permitted"

**Check Cgroup Version:**

```bash
# Check cgroup version
mount | grep cgroup

# v1: type cgroup
# v2: type cgroup2
```

**Cgroup v2 RT Configuration:**

```bash
# Check current RT bandwidth
cat /sys/fs/cgroup/cpu.rt_runtime_us
cat /sys/fs/cgroup/cpu.rt_period_us

# Default: 950000 (950ms) / 1000000 (1s) = 95% RT bandwidth

# Allow unlimited RT time (disable throttling)
echo -1 | sudo tee /sys/fs/cgroup/cpu.rt_runtime_us
```

**Docker-Specific (if using systemd cgroup driver):**

Edit `/etc/docker/daemon.json`:

```json
{
  "exec-opts": ["native.cgroupdriver=cgroupfs"],
  "default-runtime": "runc"
}
```

Restart Docker:

```bash
sudo systemctl restart docker
```

**Alternative: Use `--cgroup-parent`**

```yaml
# docker-compose.yml
services:
  virtual-testset:
    cgroup_parent: /
```

**Kubernetes:** Set `cpu.rt_runtime_us` in pod cgroup:

```yaml
apiVersion: v1
kind: Pod
metadata:
  annotations:
    cgroup.resources.beta.kubernetes.io/cpu: '{"rt_runtime_us": -1}'
```

---

### Network Mode Comparison Table

| Feature | Host | Macvlan | SR-IOV VF |
|---------|------|---------|-----------|
| **Performance** | ★★★★★ | ★★★★☆ | ★★★★★ |
| **Isolation** | ☆☆☆☆☆ | ★★★☆☆ | ★★★★★ |
| **Complexity** | ★☆☆☆☆ | ★★★☆☆ | ★★★★★ |
| **AF_PACKET** | ✅ Yes | ✅ Yes | ✅ Yes |
| **HW Timestamps** | ✅ Yes | ⚠️ Maybe | ✅ Yes |
| **PTP Device** | ✅ Direct | ⚠️ Passthrough | ✅ Per-VF |
| **Multiple Containers** | ❌ Conflicts | ✅ Yes | ✅ Yes |
| **Promisc Mode** | ✅ Yes | ⚠️ Limited | ✅ Yes |
| **VLAN Support** | ✅ Yes | ✅ Yes | ✅ Hardware |
| **QoS/Rate Limit** | ❌ No | ❌ No | ✅ Hardware |
| **Port Security** | N/A | ⚠️ May conflict | ✅ Per-VF |

---

### Smoke Test Procedures

#### Test 1: Host Networking

```bash
# Start container
docker compose up -d

# Verify interface
docker exec virtual-testset ip link show eth0

# Send test packet
docker exec virtual-testset tcpdump -i eth0 -c 1 -nn

# Expected: Sees host traffic
```

#### Test 2: Macvlan

```bash
# Create network
docker network create -d macvlan \
  --subnet=192.168.100.0/24 \
  -o parent=eth0 vts_macvlan

# Start container
docker compose -f docker-compose-macvlan.yml up -d

# Verify MAC address
docker exec vts-1 ip link show eth0 | grep link/ether

# Test connectivity
ping 192.168.100.130

# Expected: Different MAC, reachable from network
```

#### Test 3: SR-IOV VF

```bash
# Enable VFs
echo 2 | sudo tee /sys/class/net/eth0/device/sriov_numvfs

# Configure VF
sudo ip link set eth0 vf 0 mac 00:11:22:33:44:50

# Move VF to container (using pipework or manual method)
# See Method A/B above

# Verify inside container
docker exec vts-1 ethtool -i eth0

# Expected: Shows VF driver (ixgbevf, i40evf, etc.)
```

---

## References

- [Docker Runtime Constraints](https://docs.docker.com/config/containers/resource_constraints/)
- [Linux Real-Time Tuning Guide](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/8/)
- [PREEMPT_RT Kernel](https://wiki.linuxfoundation.org/realtime/start)
- [IEC 61850-90-5](https://webstore.iec.ch/publication/6028)
- [Docker Macvlan Networks](https://docs.docker.com/network/macvlan/)
- [SR-IOV Configuration Guide](https://wiki.archlinux.org/title/PCI_passthrough_via_OVMF#Setting_up_IOMMU)
- [Intel SR-IOV Driver](https://www.intel.com/content/www/us/en/support/articles/000005722/ethernet-products.html)
- [Cgroup v2 Documentation](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)

---

## Support

For issues, see:
- `AGENT_PROGRESS_VTS.txt` - Development log
- `improvements.md` - Roadmap and known issues
- GitHub Issues (if applicable)

---

**Last Updated**: Phase 10 - NIC Attachment Options (host/macvlan/SR-IOV)
