
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
  add_dependencies(${tgt} cereal::cereal)
  add_dependencies(${tgt} fmt::fmt)
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
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:cereal::cereal,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${tgt} SYSTEM PRIVATE $<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>)

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
  target_link_libraries(${tgt} PRIVATE cereal::cereal)
  target_link_libraries(${tgt} PRIVATE fmt::fmt)

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

  set(TESTS tests/unit/resources.cpp
            tests/unit/config.cpp
            tests/unit/process.cpp
            tests/unit/deployment.cpp
            tests/unit/proc_swap.cpp
  )
  foreach(test ${TESTS})

    string(REPLACE "." "_" test_name ${test})
    string(REPLACE "/" "_" test_name ${test})
    add_executable(${test_name} ${test})
    control_plane_dependencies(${test_name})
    target_link_libraries(${test_name} PRIVATE control_plane_lib)
    target_link_libraries(${test_name} PRIVATE GTest::gtest_main)
    target_link_libraries(${test_name} PRIVATE GTest::gmock_main)
    control_plane_link(${test_name})

    gtest_discover_tests(${test_name})
  endforeach()

endif()

