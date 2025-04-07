
cmake_minimum_required(VERSION 3.5)

project(Virtual-TestSet VERSION 1.0)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/build)

# find_package(nlohmann_json 3.2.0 REQUIRED)

# Subdirectories
add_subdirectory(src/main)
add_subdirectory(src/protocols)
add_subdirectory(src/tools)
add_subdirectory(src/sampledValue)
add_subdirectory(src/tests)
add_subdirectory(src/sniffer)


file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/files
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

