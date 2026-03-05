cmake_minimum_required(VERSION 3.15)
project(clox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

option(DEBUG_TRACE_EXECUTION "Trace VM instruction execution" OFF)

add_executable(clox src/main.cpp src/chunk.cpp src/debug.cpp src/value.cpp src/vm.cpp src/compiler.cpp src/scanner.cpp)
target_compile_definitions(clox PRIVATE DOCTEST_CONFIG_DISABLE)
if(DEBUG_TRACE_EXECUTION)
  target_compile_definitions(clox PRIVATE DEBUG_TRACE_EXECUTION)
endif()

add_executable(test src/test_runner.cpp src/chunk.cpp src/debug.cpp src/value.cpp src/vm.cpp src/compiler.cpp src/scanner.cpp)
