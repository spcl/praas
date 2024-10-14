cmake_minimum_required(VERSION 3.11)
project(praas-benchmarks)

find_package(runtime)

add_library(state SHARED state.cpp)
target_link_libraries(state PRIVATE praas::runtime)
target_link_libraries(state PRIVATE praas::cereal)
set_target_properties(state PROPERTIES LIBRARY_OUTPUT_DIRECTORY functions)
set(FUNCTIONS_DIRECTORY ${CMAKE_BINARY_DIR})
configure_file(functions.json.in functions/functions.json @ONLY)
