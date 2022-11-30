cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
cmake_policy(VERSION 2.8)
project(vkpreemption)

message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

include_directories(glm)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

file(GLOB EXAMPLE_SRC "*.cpp" "*.hpp")
add_executable(vkpreemption ${EXAMPLE_SRC})

target_link_libraries(
    vkpreemption
    libvulkan.so
)
