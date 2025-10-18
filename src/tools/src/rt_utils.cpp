#include "rt_utils.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>

// Linux-specific headers (will fail on non-Linux platforms)
#ifdef __linux__
#include <sys/mman.h>      // mlockall, MCL_CURRENT, MCL_FUTURE
#include <sched.h>         // sched_setscheduler, sched_param, SCHED_FIFO
#include <pthread.h>       // pthread_setaffinity_np
#include <time.h>          // clock_nanosleep, CLOCK_MONOTONIC, TIMER_ABSTIME
#include <unistd.h>        // sysconf, _SC_NPROCESSORS_ONLN
#include <fcntl.h>         // open
#include <sys/stat.h>      // open
#endif

bool rt_lock_memory() {
#ifdef __linux__
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "[RT] Warning: mlockall() failed: " << std::strerror(errno) 
                  << " (requires CAP_IPC_LOCK or root)" << std::endl;
        return false;
    }
    std::cout << "[RT] Memory locked (mlockall MCL_CURRENT | MCL_FUTURE)" << std::endl;
    return true;
#else
    std::cerr << "[RT] Warning: rt_lock_memory() not supported on non-Linux platforms" << std::endl;
    return false;
#endif
}

bool rt_set_realtime(int priority) {
#ifdef __linux__
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
#else
    std::cerr << "[RT] Warning: rt_set_realtime() not supported on non-Linux platforms" << std::endl;
    return false;
#endif
}

bool rt_set_affinity(const std::vector<int>& cpu_ids) {
#ifdef __linux__
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
#else
    std::cerr << "[RT] Warning: rt_set_affinity() not supported on non-Linux platforms" << std::endl;
    return false;
#endif
}

bool rt_sleep_abs(uint64_t target_ns) {
#ifdef __linux__
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
#else
    std::cerr << "[RT] Warning: rt_sleep_abs() not supported on non-Linux platforms" << std::endl;
    return false;
#endif
}

int rt_open_phc(const char* ptp_device) {
#ifdef __linux__
    int fd = open(ptp_device, O_RDWR);
    if (fd < 0) {
        std::cerr << "[RT] Warning: Failed to open PTP device " << ptp_device << ": " 
                  << std::strerror(errno) << std::endl;
        return -1;
    }
    
    std::cout << "[RT] Opened PTP hardware clock: " << ptp_device << " (fd=" << fd << ")" << std::endl;
    return fd;
#else
    std::cerr << "[RT] Warning: rt_open_phc() not supported on non-Linux platforms" << std::endl;
    return -1;
#endif
}
