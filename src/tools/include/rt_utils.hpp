#ifndef RT_UTILS_HPP
#define RT_UTILS_HPP

#include <vector>
#include <cstdint>

// Phase 7: Real-Time Utilities (Linux-specific)
// These functions provide real-time capabilities for time-critical operations.
// Note: Most functions require elevated privileges (CAP_SYS_NICE or root).

/**
 * Lock all current and future memory pages to prevent paging.
 * Uses mlockall(MCL_CURRENT | MCL_FUTURE).
 * Returns: true on success, false on failure (logs error).
 * Requires: CAP_IPC_LOCK capability or root privileges.
 */
bool rt_lock_memory();

/**
 * Set the calling thread to SCHED_FIFO with specified priority.
 * Priority range: 1-99 (higher = more urgent).
 * Returns: true on success, false on failure (logs error).
 * Requires: CAP_SYS_NICE capability or root privileges.
 */
bool rt_set_realtime(int priority);

/**
 * Set CPU affinity for the calling thread.
 * cpu_ids: Vector of CPU core IDs to bind to (e.g., {0, 1, 2}).
 * Returns: true on success, false on failure (logs warning, non-fatal).
 * Note: Silently ignores invalid CPU IDs.
 */
bool rt_set_affinity(const std::vector<int>& cpu_ids);

/**
 * Absolute sleep using CLOCK_MONOTONIC with EINTR retry.
 * target_ns: Target time in nanoseconds (from CLOCK_MONOTONIC).
 * Returns: true on successful sleep, false on error.
 * Automatically retries if interrupted by signal (EINTR).
 */
bool rt_sleep_abs(uint64_t target_ns);

/**
 * Optional: Open PTP hardware clock device (e.g., /dev/ptp0).
 * ptp_device: Path to PTP device (default: "/dev/ptp0").
 * Returns: File descriptor on success, -1 on failure.
 * Caller responsible for closing fd with close().
 */
int rt_open_phc(const char* ptp_device = "/dev/ptp0");

#endif // RT_UTILS_HPP
