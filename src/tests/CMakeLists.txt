

project(tests)

add_library(${PROJECT_NAME} 
    src/transient.cpp
    src/tests.cpp
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        tools
        sampledValue
        protocols
        sniffer
)