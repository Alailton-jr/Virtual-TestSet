

project(Main)

add_executable(${PROJECT_NAME} src/main.cpp)

# Phase 11: Link with platform library for compat.hpp and platform detection
target_link_libraries(${PROJECT_NAME}
    platform
    protocols
    tools
    sampledValue
    tests
    sniffer
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/include
)