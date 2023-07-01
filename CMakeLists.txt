
cmake_minimum_required(VERSION 3.11)
project(praas)

###
# Generic configuration
###
include(FetchContent)
include(FeatureSummary)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# disable checking for fetch content updates in every reconfiguration
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
endif()

###
# CMake options
###
set(PRAAS_COMPONENTS "" CACHE STRING "Components to build")
set(PRAAS_DEPLOYMENTS "" CACHE STRING "Deployments to to build")
set(PRAAS_BACKENDS "" CACHE STRING "Backends to build")
option(WITH_CXX_LINTING "Enable linting and formatting of C++ options." OFF)
option(PRAAS_WITH_TESTING "Enable testing" OFF)

###
# C++ linting
###
if(${WITH_CXX_LINTING})

  find_program(
    CLANG_TIDY_COMMAND NAMES
    clang-tidy clang-tidy-14 clang-tidy-13 clang-tidy-12
    clang-tidy-11 clang-tidy-10
  )

  if(NOT CLANG_TIDY_COMMAND)
    message(FATAL_ERROR "clang-tidy is not found!")
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
  endif()

  # We read tidy checks from the configuration file.
endif()

###
# Warnings
###
add_compile_options(-Wall -Wextra -pedantic)

foreach(COMPONENT ${PRAAS_DEPLOYMENTS})

  if(COMPONENT STREQUAL "aws")
    message(STATUS "Building PraaS deployment to AWS cloud.")
    set(PRAAS_WITH_DEPLOYMENT_AWS TRUE)
    add_compile_definitions(WITH_AWS_DEPLOYMENT)
    add_feature_info(deployments ON "aws")
  else()
    message(FATAL_ERROR "The deployment ${COMPONENT} does not exist")
  endif()

endforeach()

foreach(COMPONENT ${PRAAS_BACKENDS})

endforeach()

###
# Generate dependencies
###
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(dependencies)

###
# Enable testing
###
if(PRAAS_WITH_TESTING)

  enable_testing()
  include(GoogleTest)
  include(AddTest)
  add_compile_definitions(PRAAS_WITH_TESTING)
  add_feature_info(testing ON "enabled")

endif()

###
# Common system library.
###
file(GLOB common_library_files "common/src/*.cpp")
add_library(common_library STATIC ${common_library_files})
add_dependencies(common_library fmt::fmt)
add_dependencies(common_library stduuid)
add_dependencies(common_library spdlog::spdlog)
add_dependencies(common_library cereal::cereal)
target_include_directories(common_library PUBLIC "common/include")
target_include_directories(common_library SYSTEM PRIVATE $<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(common_library SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(common_library SYSTEM INTERFACE $<TARGET_PROPERTY:stduuid,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(common_library SYSTEM PUBLIC $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(common_library SYSTEM PUBLIC $<TARGET_PROPERTY:Drogon::Drogon,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(common_library PROPERTIES RUNTIME_OUTPUT_DIRECTORY lib)
target_link_libraries(common_library PRIVATE fmt::fmt)
target_link_libraries(common_library PRIVATE cereal::cereal)
target_link_libraries(common_library PRIVATE spdlog::spdlog)
target_link_libraries(common_library PRIVATE stduuid)
target_link_libraries(common_library PRIVATE Drogon::Drogon)

if(PRAAS_WITH_TESTING)

  # enable memcheck
  include (CTest)

  set(TESTS common/tests/unit/messages.cpp common/tests/unit/uuid.cpp)
  foreach(test ${TESTS})
    PraaS_AddTest("control-plane" test_name ${test})
    add_dependencies(${test_name} common_library)
    target_include_directories(${test_name} PRIVATE common_library)
    target_link_libraries(${test_name} PRIVATE common_library)
  endforeach()

endif()

if(NOT PRAAS_DEPLOYMENTS)

  message(FATAL_ERROR "Must specify at least one deployment with -DPRAAS_DEPLOYMENTS=...")

endif()

foreach(COMPONENT ${PRAAS_COMPONENTS})

  if(COMPONENT STREQUAL "control-plane")
    message(STATUS "Building control-plane component")
    add_subdirectory(control-plane)
    add_feature_info(component_control_plane ON "enabled")
  elseif(COMPONENT STREQUAL "sdk")
    message(STATUS "Building sdk component")
    add_subdirectory(sdk)
    add_feature_info(component_sdk ON "enabled")
  elseif(COMPONENT STREQUAL "process")
    message(STATUS "Building process component")
    add_subdirectory(process)
    add_feature_info(component_process ON "enabled")
  elseif(COMPONENT STREQUAL "benchmarks")
    message(STATUS "Building benchmarks component")
    add_subdirectory(benchmarks)
    add_feature_info(component_benchmarks ON "enabled")
  else()
    message(FATAL_ERROR "The component ${COMPONENT} does not exist")
  endif()

endforeach()

#if(PRAAS_WITH_TESTING)
#
#  set(TESTS tests/integration/invocation.cpp)
#  foreach(test ${TESTS})
#
#    PraaS_AddTest("praas" test_name ${test})
#    target_link_libraries(${test_name} PRIVATE common_library)
#    target_link_libraries(${test_name} PRIVATE controller_lib)
#    target_link_libraries(${test_name} PRIVATE praas_sdk)
#    target_link_libraries(${test_name} PRIVATE control_plane_lib)
#
#  endforeach()
#
#endif()

feature_summary(WHAT ALL)

