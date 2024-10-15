
cmake_minimum_required(VERSION 3.11)
project(praas LANGUAGES CXX VERSION 0.1)
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

###
# Runtime library
###

file(GLOB runtime_lib_files "runtime/src/*.cpp" "runtime/src/internal/*.cpp")
add_library(
  runtime
  SHARED
  ${runtime_lib_files}
)
target_include_directories(
  runtime
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/runtime/include>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(
  runtime
  SYSTEM
  PUBLIC
  $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(
  runtime
  PUBLIC
  rt
)
target_link_libraries(
  runtime
  PRIVATE
  praas::common
)
set_property(TARGET runtime PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(runtime_internal_interface INTERFACE)
target_include_directories(
  runtime_internal_interface
  INTERFACE
  $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:common_library_interface,INTERFACE_INCLUDE_DIRECTORIES>
)
#target_link_libraries(runtime PUBLIC runtime_library_interface)

###
# Process
###

# Split across library and CLI exe to allow for testing

file(GLOB controller_files "controller/src/*.cpp")
get_filename_component(fullpath_cli ${CMAKE_CURRENT_SOURCE_DIR}/controller/src/cli.cpp ABSOLUTE)
list(REMOVE_ITEM controller_files ${fullpath_cli})

add_library(controller_lib STATIC ${controller_files})
target_include_directories(controller_lib PUBLIC "controller/include")
target_include_directories(controller_lib SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(controller_lib PUBLIC runtime)
target_link_libraries(controller_lib PUBLIC runtime_internal_interface)
target_link_libraries(controller_lib PUBLIC common_library)
target_link_libraries(controller_lib PUBLIC Drogon::Drogon)

add_executable(process_exe controller/src/cli.cpp)
set_target_properties(process_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(process_exe PRIVATE controller_lib)

if(PRAAS_WITH_DEPLOYMENT_AWS)

  target_include_directories(controller_lib PRIVATE ${AWSSDK_INCLUDE_DIRS})
  target_link_libraries(controller_lib PRIVATE ${AWSSDK_LIBRARIES})

endif()

###
# C++ invoker
###

add_executable(cpp_invoker_exe invoker/cpp/cli.cpp invoker/cpp/opts.cpp invoker/cpp/functions.cpp)
target_include_directories(cpp_invoker_exe SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(cpp_invoker_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(cpp_invoker_exe PRIVATE runtime_internal_interface)
target_link_libraries(cpp_invoker_exe PRIVATE runtime)
target_link_libraries(cpp_invoker_exe PRIVATE dl)
set_target_properties(cpp_invoker_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/invoker")

add_feature_info(invoker_cpp ON "enabled")

###
# Python invoker
###
find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)

if(Python_FOUND AND pybind11_FOUND)

    message(STATUS "Building Python invoker.")
    set(PRAAS_WITH_INVOKER_PYTHON TRUE)
    add_compile_definitions(PRAAS_WITH_INVOKER_PYTHON)
    add_feature_info(invoker_python ON "enabled")

    pybind11_add_module(_pypraas invoker/python/bindings/function.cpp invoker/python/bindings/invoker.cpp)
    target_link_libraries(_pypraas PRIVATE runtime)
    target_link_libraries(_pypraas PRIVATE runtime_internal_interface)

    set_target_properties(_pypraas PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pypraas)

    configure_file(invoker/python/pypraas/__init__.py ${CMAKE_CURRENT_BINARY_DIR}/pypraas/__init__.py COPYONLY)
    configure_file(invoker/python/pypraas/buffer.py ${CMAKE_CURRENT_BINARY_DIR}/pypraas/buffer.py COPYONLY)

    configure_file(invoker/python/cli/cli.py ${CMAKE_CURRENT_BINARY_DIR}/bin/invoker/python.py COPYONLY)

else()

    set(PRAAS_WITH_INVOKER_PYTHON FALSE)
    add_feature_info(invoker_python ON "disabled")

endif()

if(PRAAS_WITH_TESTING)
  add_subdirectory(tests)
endif()


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set_target_properties(cpp_invoker_exe PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/praas")
set_target_properties(process_exe PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/praas")

target_sources(
  runtime 
  PUBLIC
  FILE_SET public_headers
  TYPE HEADERS
  BASE_DIRS runtime/include
  FILES
    runtime/include/praas/process/runtime/context.hpp
    runtime/include/praas/process/runtime/invocation.hpp
    runtime/include/praas/process/runtime/buffer.hpp
    runtime/include/praas/process/runtime/internal/buffer.hpp
)
install(
  #TARGETS process_exe runtime cereal spdlog stduuid common_library_interface
  TARGETS process_exe runtime cereal #spdlog stduuid common_library_interface
  EXPORT runtimeTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/praas
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/praas
  FILE_SET public_headers
)
install(
  TARGETS cpp_invoker_exe
  EXPORT runtimeTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/praas
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/invoker
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/praas
  FILE_SET public_headers
)
install(
  EXPORT runtimeTargets
  FILE runtimeTargets.cmake
  NAMESPACE praas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/praas
)

if(${PRAAS_WITH_INVOKER_PYTHON})

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/invoker/python/cli/cli.py DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/invoker/" RENAME "python.py")

  set_target_properties(_pypraas PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/praas")
  install(
    TARGETS _pypraas
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/pypraas
  )
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/invoker/python/pypraas/__init__.py DESTINATION "${CMAKE_INSTALL_LIBDIR}/pypraas/")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/invoker/python/pypraas/buffer.py DESTINATION "${CMAKE_INSTALL_LIBDIR}/pypraas/")

endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json DESTINATION "${CMAKE_INSTALL_PREFIX}/config")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/config_python.json DESTINATION "${CMAKE_INSTALL_PREFIX}/config")

set(version 0.1)
set_property(
  TARGET runtime
  PROPERTY VERSION
  ${version}
)
set_property(
  TARGET runtime
  PROPERTY SOVERSION 0
)
set_property(
  TARGET runtime
  PROPERTY INTERFACE_runtime_MAJOR_VERSION 0
)
set_property(
  TARGET runtime
  APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING runtime_MAJOR_VERSION
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/runtimeConfigVersion.cmake"
  VERSION "${version}"
  COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/runtimeConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/praas
)
install(
  FILES
  "${CMAKE_CURRENT_BINARY_DIR}/runtimeConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/runtimeConfigVersion.cmake"
  DESTINATION
  ${CMAKE_INSTALL_LIBDIR}/cmake/praas
)
# build tree
export(
  EXPORT
  runtimeTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/runtimeTargets.cmake"
  NAMESPACE praas::
)
