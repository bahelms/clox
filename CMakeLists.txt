cmake_minimum_required(VERSION 3.15)
project(clox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(clox src/main.cpp src/chunk.cpp src/debug.cpp src/value.cpp)
target_compile_definitions(clox PRIVATE DOCTEST_CONFIG_DISABLE)

add_executable(test src/test_runner.cpp src/chunk.cpp src/debug.cpp src/value.cpp)
