#include "rt_utils.hpp"
#include "compat.hpp"
#include "logger.hpp"
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>

// Linux-specific headers
#ifdef VTS_PLATFORM_LINUX
#include <sys/mman.h>      // mlockall, MCL_CURRENT, MCL_FUTURE
#include <sched.h>         // sched_setscheduler, sched_param, SCHED_FIFO
#include <pthread.h>       // pthread_setaffinity_np
#include <time.h>          // clock_nanosleep, CLOCK_MONOTONIC, TIMER_ABSTIME
#include <unistd.h>        // sysconf, _SC_NPROCESSORS_ONLN
#include <fcntl.h>         // open
#include <sys/stat.h>      // open
#endif

bool rt_lock_memory() {
#ifdef VTS_PLATFORM_LINUX
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        LOG_WARN("RT", "mlockall() failed: %s (requires CAP_IPC_LOCK or root)", std::strerror(errno));
        return false;
    }
    LOG_INFO("RT", "Memory locked (mlockall MCL_CURRENT | MCL_FUTURE)");
    return true;
#elif defined(VTS_PLATFORM_MAC)
    LOG_INFO("RT", "rt_lock_memory() not available on macOS (no-op)");
    return false;
#else
    LOG_INFO("RT", "rt_lock_memory() not supported on this platform (no-op)");
    return false;
#endif
}

bool rt_set_realtime(int priority) {
#ifdef VTS_PLATFORM_LINUX
    struct sched_param sp = {};
    sp.sched_priority = (priority > 0 && priority <= 99) ? priority : 80;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        LOG_WARN("RT", "sched_setscheduler(SCHED_FIFO, priority=%d) failed: %s (requires CAP_SYS_NICE or root)", 
                 sp.sched_priority, std::strerror(errno));
        return false;
    }
    LOG_INFO("RT", "Set SCHED_FIFO with priority=%d", sp.sched_priority);
    return true;
#elif defined(VTS_PLATFORM_MAC)
    (void)priority;  // Unused on macOS
    LOG_INFO("RT", "rt_set_realtime() not supported on this platform (no-op)");
    return false;
#else
    (void)priority;  // Unused on non-Linux platforms
    LOG_INFO("RT", "rt_set_realtime() not supported on this platform (no-op)");
    return false;
#endif
}

bool rt_set_affinity(const std::vector<int>& cpu_ids) {
#ifdef VTS_PLATFORM_LINUX
    if (cpu_ids.empty()) {
        LOG_WARN("RT", "Empty CPU affinity list");
        return false;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Get number of available CPUs
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus <= 0) {
        num_cpus = 1; // fallback
    }
    
    // Add valid CPU IDs to the set
    bool any_valid = false;
    for (int cpu_id : cpu_ids) {
        if (cpu_id >= 0 && cpu_id < num_cpus) {
            CPU_SET(cpu_id, &cpuset);
            any_valid = true;
        } else {
            LOG_WARN("RT", "Ignoring invalid CPU ID %d (valid range: 0-%ld)", cpu_id, num_cpus - 1);
        }
    }
    
    if (!any_valid) {
        LOG_WARN("RT", "No valid CPU IDs in affinity list");
        return false;
    }
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        LOG_WARN("RT", "pthread_setaffinity_np() failed: %s", std::strerror(errno));
        return false;
    }
    
    // Build CPU list string for logging
    std::string cpu_list;
    for (int cpu_id : cpu_ids) {
        if (cpu_id >= 0 && cpu_id < num_cpus) {
            if (!cpu_list.empty()) cpu_list += " ";
            cpu_list += std::to_string(cpu_id);
        }
    }
    LOG_INFO("RT", "CPU affinity set to: %s", cpu_list.c_str());
    return true;
#elif defined(VTS_PLATFORM_MAC)
    // Build CPU list string for logging
    std::string cpu_list;
    for (size_t i = 0; i < cpu_ids.size(); ++i) {
        if (i > 0) cpu_list += ",";
        cpu_list += std::to_string(cpu_ids[i]);
    }
    LOG_INFO("RT", "rt_set_affinity(cpus=[%s]) not available on macOS (no-op)", cpu_list.c_str());
    return false;
#else
    LOG_INFO("RT", "rt_set_affinity() not supported on this platform (no-op)");
    return false;
#endif
}

bool rt_sleep_abs(uint64_t target_ns) {
#ifdef VTS_PLATFORM_LINUX
    struct timespec target_ts;
    target_ts.tv_sec = static_cast<time_t>(target_ns / 1000000000ULL);
    target_ts.tv_nsec = static_cast<long>(target_ns % 1000000000ULL);
    
    int result;
    do {
        result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target_ts, nullptr);
        // Retry if interrupted by signal (EINTR)
    } while (result == EINTR);
    
    if (result != 0) {
        LOG_ERROR("RT", "clock_nanosleep() failed: %s", std::strerror(result));
        return false;
    }
    
    return true;
#elif defined(VTS_PLATFORM_MAC)
    // On macOS, use nanosleep as a fallback (not absolute, but functional for no-op mode)
    LOG_INFO("RT", "rt_sleep_abs() using nanosleep fallback on macOS");
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(target_ns / 1000000000ULL);
    ts.tv_nsec = static_cast<long>(target_ns % 1000000000ULL);
    
    int result;
    do {
        result = nanosleep(&ts, &ts);
    } while (result == -1 && errno == EINTR);
    
    return (result == 0);
#else
    LOG_INFO("RT", "rt_sleep_abs() not supported on this platform (no-op)");
    return false;
#endif
}

int rt_open_phc(const char* ptp_device) {
#ifdef VTS_PLATFORM_LINUX
    int fd = open(ptp_device, O_RDWR);
    if (fd < 0) {
        LOG_WARN("RT", "Failed to open PTP device %s: %s", ptp_device, std::strerror(errno));
        return -1;
    }
    
    LOG_INFO("RT", "Opened PTP hardware clock: %s (fd=%d)", ptp_device, fd);
    return fd;
#elif defined(VTS_PLATFORM_MAC)
    LOG_INFO("RT", "rt_open_phc(%s) not available on macOS (no-op)", ptp_device);
    return -1;
#else
    LOG_INFO("RT", "rt_open_phc() not supported on this platform (no-op)");
    return -1;
#endif
}
