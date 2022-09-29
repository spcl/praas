
cmake_minimum_required(VERSION 3.11)
project(praas)

###
# Generic configuration
###
include(FetchContent)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

###
# CMake options
###
option(WITH_CONTROL_PLANE "Build control plane deployment" ON)
option(WITH_CXX_LINTING "Enable linting and formatting of C++ options." OFF)
option(WITH_TESTING "Enable testing" OFF)

###
# C++ linting
### 
if(${WITH_CXX_LINTING})
  find_program(CLANG_TIDY_COMMAND NAMES clang-tidy clang-tidy-14 clang-tidy-13 clang-tidy-12 clang-tidy-11 clang-tidy-10)
  if(NOT CLANG_TIDY_COMMAND)
    message(FATAL_ERROR "clang-tidy is not found!")
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
  else()
    set(CLANG_TIDY_CHECKS "*")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND};-checks=${CLANG_TIDY_CHECKS};-header-filter=.")
  endif()
endif()

###
# Warnings
###
add_compile_options(-Wall -Wextra -pedantic)

###
# Generate dependencies
###
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(dependencies)

###
# Common system library.
###
file(GLOB common_library_files "common/src/*.cpp")
add_library(common_library STATIC ${common_library_files})
target_include_directories(common_library PUBLIC "common/include")
set_target_properties(common_library PROPERTIES RUNTIME_OUTPUT_DIRECTORY lib)

if(${WITH_CONTROL_PLANE})
  add_subdirectory(control-plane)
endif()

