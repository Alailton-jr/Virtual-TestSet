#include "rt_utils.hpp"
#include "compat.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>

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
        std::cerr << "[RT] Warning: mlockall() failed: " << std::strerror(errno) 
                  << " (requires CAP_IPC_LOCK or root)" << std::endl;
        return false;
    }
    std::cout << "[RT] Memory locked (mlockall MCL_CURRENT | MCL_FUTURE)" << std::endl;
    return true;
#elif defined(VTS_PLATFORM_MAC)
    std::cout << "[RT] INFO: rt_lock_memory() not available on macOS (no-op)" << std::endl;
    return false;
#else
    std::cout << "[RT] INFO: rt_lock_memory() not supported on this platform (no-op)" << std::endl;
    return false;
#endif
}

bool rt_set_realtime(int priority) {
#ifdef VTS_PLATFORM_LINUX
    // Validate priority range for SCHED_FIFO (typically 1-99)
    if (priority < 1 || priority > 99) {
        std::cerr << "[RT] Error: Invalid priority " << priority << " (valid range: 1-99)" << std::endl;
        return false;
    }

    struct sched_param param;
    param.sched_priority = priority;
    
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        std::cerr << "[RT] Warning: sched_setscheduler(SCHED_FIFO, " << priority << ") failed: " 
                  << std::strerror(errno) << " (requires CAP_SYS_NICE or root)" << std::endl;
        return false;
    }
    
    std::cout << "[RT] Thread set to SCHED_FIFO with priority " << priority << std::endl;
    return true;
#elif defined(VTS_PLATFORM_MAC)
    std::cout << "[RT] INFO: rt_set_realtime(priority=" << priority << ") not available on macOS (no-op)" << std::endl;
    return false;
#else
    std::cout << "[RT] INFO: rt_set_realtime() not supported on this platform (no-op)" << std::endl;
    return false;
#endif
}

bool rt_set_affinity(const std::vector<int>& cpu_ids) {
#ifdef VTS_PLATFORM_LINUX
    if (cpu_ids.empty()) {
        std::cerr << "[RT] Warning: Empty CPU affinity list" << std::endl;
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
            std::cerr << "[RT] Warning: Ignoring invalid CPU ID " << cpu_id 
                      << " (valid range: 0-" << (num_cpus - 1) << ")" << std::endl;
        }
    }
    
    if (!any_valid) {
        std::cerr << "[RT] Warning: No valid CPU IDs in affinity list" << std::endl;
        return false;
    }
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[RT] Warning: pthread_setaffinity_np() failed: " 
                  << std::strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[RT] CPU affinity set to: ";
    for (int cpu_id : cpu_ids) {
        if (cpu_id >= 0 && cpu_id < num_cpus) {
            std::cout << cpu_id << " ";
        }
    }
    std::cout << std::endl;
    return true;
#elif defined(VTS_PLATFORM_MAC)
    std::cout << "[RT] INFO: rt_set_affinity(cpus=[";
    for (size_t i = 0; i < cpu_ids.size(); ++i) {
        std::cout << cpu_ids[i];
        if (i + 1 < cpu_ids.size()) std::cout << ",";
    }
    std::cout << "]) not available on macOS (no-op)" << std::endl;
    return false;
#else
    std::cout << "[RT] INFO: rt_set_affinity() not supported on this platform (no-op)" << std::endl;
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
        std::cerr << "[RT] Error: clock_nanosleep() failed: " << std::strerror(result) << std::endl;
        return false;
    }
    
    return true;
#elif defined(VTS_PLATFORM_MAC)
    // On macOS, use nanosleep as a fallback (not absolute, but functional for no-op mode)
    std::cout << "[RT] INFO: rt_sleep_abs() using nanosleep fallback on macOS" << std::endl;
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(target_ns / 1000000000ULL);
    ts.tv_nsec = static_cast<long>(target_ns % 1000000000ULL);
    
    int result;
    do {
        result = nanosleep(&ts, &ts);
    } while (result == -1 && errno == EINTR);
    
    return (result == 0);
#else
    std::cout << "[RT] INFO: rt_sleep_abs() not supported on this platform (no-op)" << std::endl;
    return false;
#endif
}

int rt_open_phc(const char* ptp_device) {
#ifdef VTS_PLATFORM_LINUX
    int fd = open(ptp_device, O_RDWR);
    if (fd < 0) {
        std::cerr << "[RT] Warning: Failed to open PTP device " << ptp_device << ": " 
                  << std::strerror(errno) << std::endl;
        return -1;
    }
    
    std::cout << "[RT] Opened PTP hardware clock: " << ptp_device << " (fd=" << fd << ")" << std::endl;
    return fd;
#elif defined(VTS_PLATFORM_MAC)
    std::cout << "[RT] INFO: rt_open_phc(" << ptp_device << ") not available on macOS (no-op)" << std::endl;
    return -1;
#else
    std::cout << "[RT] INFO: rt_open_phc() not supported on this platform (no-op)" << std::endl;
    return -1;
#endif
}
