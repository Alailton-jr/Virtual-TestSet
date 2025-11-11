**“Update the codebase so that it becomes fully cross-platform while preserving real-time optimizations on Linux. The system must run inside Docker on macOS, Windows, and Linux.**

**Requirements:**

1. **Cross-Platform Execution**

   * The application must run correctly on macOS and Windows Docker environments, even without Linux real-time features.
   * All core functionalities of the Virtual TestSet must remain available, regardless of platform.

2. **Adaptive Real-Time Behavior**

   * When the container is running on a Linux host, automatically detect the environment and enable high-performance paths:

     * Use RT-kernel capabilities.
     * Apply CPU isolation, taskset/cset, SCHED_FIFO/SCHED_RR, `isolcpus`, and other Linux-specific optimizations.
     * Allow optional elevated capabilities (`CAP_SYS_NICE`, `CAP_SYS_RAWIO`, etc.).

   * When the container runs on macOS or Windows:

     * Fall back to standard OS scheduling.
     * Emulate RT-like behavior as closely as possible (high-priority threads, affinity where available, optimized loops).
     * Ensure graceful degradation without breaking functionality.

3. **Conditional Code Paths**

   * Refactor all Linux-specific code (thread priorities, SCHED_FIFO, real-time queues, CPU shielding, etc.) behind clean abstraction layers.
   * Implement OS detection and enable/disable features accordingly.
   * The code must never crash due to missing Linux RT features.

4. **Performance Philosophy**

   * On Linux servers: run at maximum speed with real-time flags and kernel-level optimizations.
   * On non-Linux hosts: run normally with best-effort performance, maintaining correctness and feature completeness.

**Deliverables:**

* Updated code.
* Platform detection layer.
* Documentation describing RT-only features, fallbacks, and behavioral differences.”**

