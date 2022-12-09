cmake_minimum_required(VERSION 3.11)
project(praas-benchmarks)

add_executable(invocations_benchmarker invocations_benchmarker.cpp)
target_link_libraries(invocations_benchmarker PUBLIC spdlog::spdlog)
target_link_libraries(invocations_benchmarker PUBLIC cereal::cereal)
target_link_libraries(invocations_benchmarker PRIVATE praas_sdk)

add_library(benchmark_functions SHARED functions/cpp/functions.cpp)
set_target_properties(benchmark_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY functions)
target_link_libraries(benchmark_functions PRIVATE function_lib)

set(PRAAS_DIRECTORY ${CMAKE_BINARY_DIR})
set(PRAAS_SOURCE_DIRECTORY ${CMAKE_SOURCE_DIR})
configure_file(config.json.in config.json @ONLY)
configure_file(functions.json.in functions.json @ONLY)
