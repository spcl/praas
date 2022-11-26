
cmake_minimum_required(VERSION 3.11)
project(praas-process)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

###
# Generic configuration
###
include(FetchContent)
include(FeatureSummary)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# disable checking for fetch content updates in every reconfiguration
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)


###
# External dependencies
###
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(dependencies)

add_compile_options(-Wall -Wextra -pedantic)

function(process_dependencies tgt)

  add_dependencies(${tgt} cxxopts::cxxopts)
  add_dependencies(${tgt} spdlog::spdlog)
  add_dependencies(${tgt} cereal::cereal)
  add_dependencies(${tgt} fmt::fmt)
  add_dependencies(${tgt} TBB::tbb)
  add_dependencies(${tgt} common_library)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PUBLIC $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:TBB::tbb,INTERFACE_INCLUDE_DIRECTORIES>)

endfunction()

function(process_link tgt)

  target_link_libraries(${tgt} PRIVATE common_library)
  target_link_libraries(${tgt} PRIVATE spdlog::spdlog)
  target_link_libraries(${tgt} PUBLIC cereal::cereal)
  target_link_libraries(${tgt} PRIVATE TBB::tbb)

endfunction()

###
# Runtime library
###

file(GLOB process_lib_files "runtime/src/*.cpp")
add_library(process_lib STATIC ${process_lib_files})
target_include_directories(process_lib SYSTEM PUBLIC "runtime/include")
process_dependencies(process_lib)
process_link(process_lib)

###
# Process
###

# Split across library and CLI exe to allow for testing

file(GLOB controller_files "controller/src/*.cpp")
get_filename_component(fullpath_cli ${CMAKE_CURRENT_SOURCE_DIR}/controller/src/cli.cpp ABSOLUTE)
list(REMOVE_ITEM controller_files ${fullpath_cli})

add_library(controller_lib STATIC ${controller_files})
target_include_directories(controller_lib SYSTEM PUBLIC "controller/include")
target_link_libraries(controller_lib PUBLIC process_lib)
process_dependencies(controller_lib)
process_link(controller_lib)

add_executable(process_exe controller/src/cli.cpp)
set_target_properties(process_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(process_exe PRIVATE controller_lib)
process_dependencies(process_exe)
process_link(process_exe)

###
# Function API
###

file(GLOB func_api_files "invoker/function/src/*.cpp")
add_library(function_lib STATIC ${func_api_files})
target_include_directories(function_lib SYSTEM PUBLIC "invoker/function/include")
target_link_libraries(function_lib PUBLIC cereal::cereal)
target_link_libraries(function_lib PUBLIC spdlog::spdlog)
target_link_libraries(function_lib PUBLIC process_lib)
#process_dependencies(function_lib)
#process_link(function_lib)

###
# Invoker library
###

file(GLOB invoker_files "invoker/src/*.cpp")
add_library(invoker_lib STATIC ${invoker_files})
target_include_directories(invoker_lib SYSTEM PUBLIC "invoker/include")
target_link_libraries(invoker_lib PUBLIC process_lib)
target_link_libraries(invoker_lib PUBLIC function_lib)
process_dependencies(invoker_lib)
process_link(invoker_lib)

###
# C++ invoker
###

add_executable(cpp_invoker_exe invoker/cpp/cli.cpp invoker/cpp/opts.cpp invoker/cpp/functions.cpp)
set_target_properties(cpp_invoker_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(cpp_invoker_exe PRIVATE invoker_lib)
process_dependencies(cpp_invoker_exe)
process_link(cpp_invoker_exe)

###
# Python invoker
###
# FIXME: Python

if(WITH_TESTING)
  add_subdirectory(tests)
endif()
