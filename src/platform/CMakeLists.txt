# Phase 11: Platform compatibility layer
# This directory provides platform-specific headers and stubs for macOS portability

add_library(platform INTERFACE)

target_include_directories(platform
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Headers are header-only, no source files to compile
# The library just provides the include path for compat.hpp and raw_socket_stub.hpp
