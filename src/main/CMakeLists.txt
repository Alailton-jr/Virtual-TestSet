

project(TestSet)

add_executable(${PROJECT_NAME} src/main.cpp)


target_link_libraries(${PROJECT_NAME}
    protocols
    tools
    sampledValue
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC 
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/include
)