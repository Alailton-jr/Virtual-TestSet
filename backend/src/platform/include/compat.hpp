#ifndef PLATFORM_COMPAT_HPP
#define PLATFORM_COMPAT_HPP

// ============================================================================
// Platform Detection and Compatibility Macros
// ============================================================================
// This header provides platform detection macros for conditional compilation
// of platform-specific code. Virtual TestSet supports:
//
// - Linux: Full functionality (raw sockets, real-time, packet I/O)
// - macOS: Limited functionality (no raw networking, no-op RT functions)
//
// Usage:
//   #ifdef VTS_PLATFORM_LINUX
//     // Linux-specific code
//   #elif defined(VTS_PLATFORM_MAC)
//     // macOS-specific code
//   #endif
// ============================================================================

// Platform detection
#if defined(__linux__)
    #define VTS_PLATFORM_LINUX
    #define VTS_PLATFORM_NAME "Linux"
    #define VTS_HAS_RAW_SOCKETS 1
    #define VTS_HAS_REALTIME 1
    #define VTS_HAS_PACKET_MMAP 1
#elif defined(__APPLE__) && defined(__MACH__)
    #define VTS_PLATFORM_MAC
    #define VTS_PLATFORM_NAME "macOS"
    #define VTS_HAS_RAW_SOCKETS 0
    #define VTS_HAS_REALTIME 0
    #define VTS_HAS_PACKET_MMAP 0
#else
    #define VTS_PLATFORM_UNKNOWN
    #define VTS_PLATFORM_NAME "Unknown"
    #define VTS_HAS_RAW_SOCKETS 0
    #define VTS_HAS_REALTIME 0
    #define VTS_HAS_PACKET_MMAP 0
    #warning "Unsupported platform detected. Building with limited functionality."
#endif

// Feature detection helpers
#ifdef VTS_PLATFORM_LINUX
    #include <linux/version.h>
    
    // Check for TPACKET_V3 support (kernel >= 3.2)
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
        #define VTS_HAS_TPACKET_V3 1
    #else
        #define VTS_HAS_TPACKET_V3 0
    #endif
#else
    #define VTS_HAS_TPACKET_V3 0
#endif

// Compiler hints for platform-specific optimizations
#ifdef VTS_PLATFORM_LINUX
    // Linux-specific compiler hints
    #define VTS_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define VTS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define VTS_PREFETCH(addr) __builtin_prefetch(addr)
#else
    // Generic fallbacks
    #define VTS_LIKELY(x)   (x)
    #define VTS_UNLIKELY(x) (x)
    #define VTS_PREFETCH(addr) ((void)(addr))
#endif

// Platform capability summary
namespace vts {
namespace platform {

struct Capabilities {
    static constexpr bool has_raw_sockets = VTS_HAS_RAW_SOCKETS;
    static constexpr bool has_realtime = VTS_HAS_REALTIME;
    static constexpr bool has_packet_mmap = VTS_HAS_PACKET_MMAP;
    static constexpr bool has_tpacket_v3 = VTS_HAS_TPACKET_V3;
    static constexpr const char* platform_name = VTS_PLATFORM_NAME;
};

// Get platform info string
inline const char* get_platform_info() {
#ifdef VTS_PLATFORM_LINUX
    return "Linux (full functionality: raw sockets, RT, TPACKET_V3)";
#elif defined(VTS_PLATFORM_MAC)
    return "macOS (limited: no raw sockets, no RT, use --no-net mode)";
#else
    return "Unknown platform (limited functionality)";
#endif
}

// Check if network operations are supported
inline bool network_operations_supported() {
#ifdef VTS_PLATFORM_LINUX
    return true;
#else
    return false;
#endif
}

// Check if real-time operations are supported
inline bool realtime_operations_supported() {
#ifdef VTS_PLATFORM_LINUX
    return true;
#else
    return false;
#endif
}

} // namespace platform
} // namespace vts

#endif // PLATFORM_COMPAT_HPP
