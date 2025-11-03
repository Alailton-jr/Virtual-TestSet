

project(api)

add_library(${PROJECT_NAME} 
    src/tcp_server.cpp
    src/http_server.cpp
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
        ${CMAKE_SOURCE_DIR}/third_party/json/include
        ${CMAKE_SOURCE_DIR}/src/core/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        vts_core
    PRIVATE
        tools
        Threads::Threads
)