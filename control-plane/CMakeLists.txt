
project(praas-control-plane)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

###
# External dependencies
###
include(dependencies)

function(control_plane_dependencies tgt)

  add_dependencies(${tgt} cxxopts::cxxopts)
  add_dependencies(${tgt} sockpp-static)
  add_dependencies(${tgt} spdlog::spdlog)
  add_dependencies(${tgt} common_library)
  add_dependencies(${tgt} Crow::Crow)
  add_dependencies(${tgt} TBB::tbb)
  target_include_directories(${tgt} SYSTEM PRIVATE "include")
  target_include_directories(${tgt} SYSTEM PRIVATE ${threadpool_SOURCE_DIR})
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:common_library,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:sockpp-static,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:redis++::redis++,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:stduuid,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:Crow::Crow,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:TBB::tbb,INTERFACE_INCLUDE_DIRECTORIES>)

endfunction()

function(control_plane_link tgt)

  target_link_libraries(${tgt} PRIVATE common_library)
  target_link_libraries(${tgt} PRIVATE spdlog::spdlog)
  target_link_libraries(${tgt} PRIVATE sockpp-static)
  target_link_libraries(${tgt} PRIVATE redis++::redis++_static)
  target_link_libraries(${tgt} PRIVATE ${HIREDIS_LIB})
  target_link_libraries(${tgt} PRIVATE stduuid)
  target_link_libraries(${tgt} PRIVATE Crow::Crow)
  target_link_libraries(${tgt} PRIVATE TBB::tbb)

endfunction()

###
# control plane
###
file(GLOB control_plane_files "src/*.cpp")
get_filename_component(fullpath_cli ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.cpp ABSOLUTE)
list(REMOVE_ITEM control_plane_files ${fullpath_cli})

add_library(control_plane_lib STATIC ${control_plane_files})
control_plane_dependencies(control_plane_lib)
control_plane_link(control_plane_lib)

add_executable(control_plane src/cli.cpp)
set_target_properties(control_plane PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
target_link_libraries(control_plane PRIVATE control_plane_lib)
control_plane_dependencies(control_plane)
control_plane_link(control_plane)

if(WITH_TESTING)

  add_executable(unit_resources tests/unit/resources.cpp)
  control_plane_dependencies(unit_resources)
  target_link_libraries(unit_resources PRIVATE control_plane_lib)
  target_link_libraries(unit_resources PRIVATE GTest::gtest_main)
  control_plane_link(unit_resources)

  gtest_discover_tests(unit_resources)

endif()

