cmake_minimum_required(VERSION 3.11)
project(praas-benchmarks)

add_executable(invocations_benchmarker invocations_benchmarker.cpp)
target_link_libraries(invocations_benchmarker PUBLIC spdlog::spdlog)
target_link_libraries(invocations_benchmarker PUBLIC cereal::cereal)
target_link_libraries(invocations_benchmarker PRIVATE praas_sdk)

add_executable(local_invocations_benchmarker local_invocations_benchmarker.cpp)
target_link_libraries(local_invocations_benchmarker PUBLIC spdlog::spdlog)
target_link_libraries(local_invocations_benchmarker PUBLIC cereal::cereal)
target_link_libraries(local_invocations_benchmarker PRIVATE praas_sdk)

add_library(benchmark_functions SHARED functions/cpp/functions.cpp)
set_target_properties(benchmark_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY functions)
target_link_libraries(benchmark_functions PRIVATE function_lib)

set(PRAAS_DIRECTORY ${CMAKE_BINARY_DIR})
set(PRAAS_SOURCE_DIRECTORY ${CMAKE_SOURCE_DIR})
configure_file(config.json.in config.json @ONLY)
configure_file(functions.json.in functions.json @ONLY)

add_library(ipc_functions SHARED ipc/functions/cpp/functions.cpp)
set_target_properties(ipc_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY functions)
target_link_libraries(ipc_functions PRIVATE function_lib)

find_package(AWSSDK COMPONENTS s3 core REQUIRED)
target_link_libraries(ipc_functions PUBLIC ${AWSSDK_LINK_LIBRARIES})

find_package(PkgConfig REQUIRED)
#set(ENV{PKG_CONFIG_PATH} "/opt/lib/pkgconfig")
pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
target_include_directories(ipc_functions PUBLIC PkgConfig::HIREDIS)
target_link_libraries(ipc_functions PUBLIC PkgConfig::HIREDIS)

add_executable(comm_benchmarker ipc/benchmaker.cpp)
target_link_libraries(comm_benchmarker PUBLIC spdlog::spdlog)
target_link_libraries(comm_benchmarker PUBLIC cereal::cereal)
target_link_libraries(comm_benchmarker PRIVATE praas_sdk)
configure_file(ipc/config.json.in ipc/config.json @ONLY)
configure_file(ipc/functions.json.in ipc/functions.json @ONLY)
