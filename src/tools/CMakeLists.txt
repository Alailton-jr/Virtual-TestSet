

project(tools)

# Phase 7: Changed from INTERFACE to static library to support rt_utils.cpp
add_library(${PROJECT_NAME} STATIC
    src/rt_utils.cpp
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)