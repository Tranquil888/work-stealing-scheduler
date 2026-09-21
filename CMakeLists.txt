cmake_minimum_required(VERSION 3.20)

project(wss VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(WSS_SANITIZE "Enable address+undefined sanitizers" OFF)
if(WSS_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

option(WSS_BUILD_TESTS "Build tests" ON)
option(WSS_BUILD_BENCHMARKS "Build benchmarks" ON)
option(WSS_BUILD_EXAMPLES "Build examples" ON)

add_library(wss STATIC
  src/task.cpp
  src/work_queue.cpp
  src/work_stealing_policy.cpp
  src/scheduling_policy.cpp
  src/worker.cpp
  src/scheduler.cpp
  src/cancellation.cpp
  src/timer.cpp
  src/observer.cpp
  src/verified.cpp
)

target_include_directories(wss PUBLIC include)

if(NOT MSVC)
  find_package(Threads REQUIRED)
  target_link_libraries(wss PUBLIC Threads::Threads)
endif()

if(WSS_BUILD_EXAMPLES)
  add_executable(wss_demo examples/demo.cpp)
  target_link_libraries(wss_demo PRIVATE wss)
endif()

if(WSS_BUILD_BENCHMARKS)
  add_executable(wss_throughput benchmarks/throughput.cpp)
  target_link_libraries(wss_throughput PRIVATE wss)
  add_executable(wss_latency benchmarks/latency.cpp)
  target_link_libraries(wss_latency PRIVATE wss)
endif()

if(WSS_BUILD_TESTS)
  enable_testing()
  set(WSS_TESTS
    task_test
    work_queue_test
    scheduler_test
    future_test
    cancellation_test
    timer_test
    verified_test
  )
  foreach(t IN LISTS WSS_TESTS)
    add_executable(${t} tests/${t}.cpp)
    target_link_libraries(${t} PRIVATE wss)
    add_test(NAME ${t} COMMAND ${t})
  endforeach()
endif()
