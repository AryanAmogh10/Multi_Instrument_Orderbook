cmake_minimum_required(VERSION 3.20)
project(velox-match
    VERSION 0.1.0
    DESCRIPTION "Options-aware multi-instrument matching engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Sanitizers)

option(VELOX_BUILD_TESTS "Build unit tests" ON)
option(VELOX_BUILD_BENCHMARKS "Build benchmarks" OFF)

include(FetchContent)

add_library(velox_core
    src/orderbook/orderbook.cpp
    src/instruments/instrument_registry.cpp
    src/instruments/contract.cpp
    src/instruments/chain.cpp
    src/matching/matcher.cpp
    src/matching/engine.cpp
    src/matching/sharded_engine.cpp
    src/matching/sweeper.cpp
    src/protocol/codec.cpp
    src/protocol/framer.cpp
    src/protocol/session.cpp
    src/gateway/dispatcher.cpp
    src/gateway/tcp.cpp
    src/gateway/server.cpp
    src/gateway/client.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(velox_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(velox_core PUBLIC ws2_32)
endif()
target_include_directories(velox_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
velox_set_warnings(velox_core)
velox_apply_sanitizers(velox_core)

add_executable(velox_engine src/main.cpp)
target_link_libraries(velox_engine PRIVATE velox_core)
velox_set_warnings(velox_engine)
velox_apply_sanitizers(velox_engine)

if(VELOX_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()

if(VELOX_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
    add_subdirectory(benchmarks)
endif()

message(STATUS "velox-match ${PROJECT_VERSION}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  Sanitizer:   ${VELOX_SANITIZER}")
message(STATUS "  Tests:       ${VELOX_BUILD_TESTS}")
message(STATUS "  Benchmarks:  ${VELOX_BUILD_BENCHMARKS}")
