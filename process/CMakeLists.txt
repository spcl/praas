
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
  "runtime/include"
)
target_include_directories(
  runtime
  SYSTEM
  PUBLIC
  $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:praas::common,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(
  runtime
  PUBLIC
  rt
  praas::common
)
set_property(TARGET runtime PROPERTY POSITION_INDEPENDENT_CODE ON)
install(TARGETS runtime EXPORT ${PROJECT_NAME})

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

add_executable(process_exe controller/src/cli.cpp)
set_target_properties(process_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(process_exe PRIVATE controller_lib)
#install(TARGETS functionsdk
#        EXPORT  ${PROJECT_NAME}
#        LIBRARY       DESTINATION "${CMAKE_INSTALL_LIBDIR}"                            COMPONENT shlib
#        ARCHIVE       DESTINATION "${CMAKE_INSTALL_LIBDIR}"                            COMPONENT lib
#        RUNTIME       DESTINATION "${CMAKE_INSTALL_BINDIR}"                            COMPONENT bin
#        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/functionsdk" COMPONENT dev)

###
# C++ invoker
###

add_executable(cpp_invoker_exe invoker/cpp/cli.cpp invoker/cpp/opts.cpp invoker/cpp/functions.cpp)
target_include_directories(cpp_invoker_exe SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(cpp_invoker_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(cpp_invoker_exe PRIVATE runtime)
target_link_libraries(cpp_invoker_exe PUBLIC dl)
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
