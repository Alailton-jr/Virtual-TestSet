

project(tools)

# Phase 7: Changed from INTERFACE to static library to support rt_utils.cpp
# Phase 8: Added packet_ring.cpp for TPACKET_V3 support
add_library(${PROJECT_NAME} STATIC
    src/rt_utils.cpp
    src/packet_ring.cpp
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)