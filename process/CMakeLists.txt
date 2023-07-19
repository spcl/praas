
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

function(process_dependencies tgt)

  #add_dependencies(${tgt} cxxopts::cxxopts)
  #add_dependencies(${tgt} spdlog::spdlog)
  #add_dependencies(${tgt} cereal::cereal)
  #add_dependencies(${tgt} fmt::fmt)
  #add_dependencies(${tgt} Drogon::Drogon)
  #add_dependencies(${tgt} common_library)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PUBLIC $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PUBLIC $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PUBLIC $<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PUBLIC $<TARGET_PROPERTY:Drogon::Drogon,INTERFACE_INCLUDE_DIRECTORIES>)

endfunction()

function(process_link tgt)

  target_link_libraries(${tgt} PUBLIC common_library)
  target_link_libraries(${tgt} PUBLIC spdlog::spdlog)
  target_link_libraries(${tgt} PUBLIC fmt::fmt)
  target_link_libraries(${tgt} PUBLIC cereal::cereal)
  target_link_libraries(${tgt} PUBLIC Drogon::Drogon)

endfunction()

function(process_link_private tgt)

  target_link_libraries(${tgt} PRIVATE common_library)
  target_link_libraries(${tgt} PRIVATE spdlog::spdlog)
  target_link_libraries(${tgt} PRIVATE fmt::fmt)
  target_link_libraries(${tgt} PRIVATE cereal::cereal)

endfunction()

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
#target_link_libraries(functionsdk PUBLIC cereal::cereal)
#target_link_libraries(functionsdk PUBLIC runtime)
set_property(TARGET runtime PROPERTY POSITION_INDEPENDENT_CODE ON)
#process_dependencies(runtime)
#process_link(runtime)
install(TARGETS runtime EXPORT ${PROJECT_NAME})

###
# Process
###

# Split across library and CLI exe to allow for testing

file(GLOB controller_files "controller/src/*.cpp")
get_filename_component(fullpath_cli ${CMAKE_CURRENT_SOURCE_DIR}/controller/src/cli.cpp ABSOLUTE)
list(REMOVE_ITEM controller_files ${fullpath_cli})

add_library(controller_lib STATIC ${controller_files})
target_include_directories(controller_lib SYSTEM PUBLIC "controller/include")
target_link_libraries(controller_lib PUBLIC runtime)
process_dependencies(controller_lib)
process_link(controller_lib)

add_executable(process_exe controller/src/cli.cpp)
set_target_properties(process_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(process_exe PRIVATE controller_lib)
process_dependencies(process_exe)
process_link(process_exe)

###
# Invoker library
###

#file(GLOB invoker_files "invoker/src/*.cpp")
#get_filename_component(fullpath_python ${CMAKE_CURRENT_SOURCE_DIR}/invoker/src/python.cpp ABSOLUTE)
#list(REMOVE_ITEM invoker_files ${fullpath_python})
#
#add_library(invoker_lib STATIC ${invoker_files})
#target_include_directories(invoker_lib SYSTEM PUBLIC "invoker/include")
#target_link_libraries(invoker_lib PUBLIC runtime)
##target_link_libraries(invoker_lib PUBLIC function_lib)
#process_dependencies(invoker_lib)
#process_link(invoker_lib)

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
set_target_properties(cpp_invoker_exe PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(cpp_invoker_exe PRIVATE runtime)
target_link_libraries(cpp_invoker_exe PUBLIC dl)
process_dependencies(cpp_invoker_exe)
process_link(cpp_invoker_exe)
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
    process_link_private(_pypraas)

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
