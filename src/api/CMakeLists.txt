

project(api)

add_library(${PROJECT_NAME} 
    src/tcp_server.cpp
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    tools
    TestSet
)