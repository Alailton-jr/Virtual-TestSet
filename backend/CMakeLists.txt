
cmake_minimum_required(VERSION 3.5)

project(Virtual-TestSet VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)

# Strict compiler warnings (Werror removed globally to allow third-party libraries)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion")

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

if(ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(ENABLE_UBSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
endif()

# Third-party dependencies
include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/asio/asio/include
)

# Define ASIO_STANDALONE to use asio without Boost
add_definitions(-DASIO_STANDALONE)

# Find fftw3 library
find_library(FFTW3_LIBRARY NAMES fftw3 PATHS /opt/homebrew/opt/fftw/lib /usr/local/lib /usr/lib)
if(NOT FFTW3_LIBRARY)
    message(FATAL_ERROR "fftw3 library not found. Please install it via: brew install fftw")
endif()
message(STATUS "Found fftw3 library: ${FFTW3_LIBRARY}")

# Find pthread library (required by websocketpp and asio)
find_package(Threads REQUIRED)

# Subdirectories
add_subdirectory(src/platform)
add_subdirectory(src/main)
add_subdirectory(src/protocols)
add_subdirectory(src/tools)
add_subdirectory(src/sampledValue)
add_subdirectory(src/tests)
add_subdirectory(src/sniffer)
add_subdirectory(src/goose)
add_subdirectory(src/api)
add_subdirectory(src/core)
add_subdirectory(src/synth)
add_subdirectory(src/io)


file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/files
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Enable testing
enable_testing()

# Phase 13: Add unit tests
add_subdirectory(tests)

# Placeholder test - binary runs with --help
add_test(NAME basic_help_test
         COMMAND Virtual-TestSet --help
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(basic_help_test PROPERTIES
                     WILL_FAIL TRUE)  # Expected to fail until --help is implemented

