
project(praas-control-plane)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

###
# External dependencies
###
include(dependencies)

###
# control plane
###
file(GLOB control_plane_files "src/*.cpp")
add_executable(control_plane ${control_plane_files})
set_target_properties(control_plane PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
add_dependencies(control_plane cxxopts::cxxopts)
add_dependencies(control_plane sockpp-static)
add_dependencies(control_plane spdlog::spdlog)
add_dependencies(control_plane common_library)
add_dependencies(control_plane Crow::Crow)
target_include_directories(control_plane SYSTEM PRIVATE "include")
target_include_directories(control_plane SYSTEM PRIVATE common_library)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:sockpp-static,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:redis++::redis++,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE ${threadpool_SOURCE_DIR})
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:stduuid,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(control_plane SYSTEM PRIVATE $<TARGET_PROPERTY:Crow::Crow,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(control_plane PRIVATE common_library)
target_link_libraries(control_plane PRIVATE spdlog::spdlog)
target_link_libraries(control_plane PRIVATE sockpp-static)
target_link_libraries(control_plane PRIVATE redis++::redis++_static)
target_link_libraries(control_plane PRIVATE ${HIREDIS_LIB})
target_link_libraries(control_plane PRIVATE stduuid)
target_link_libraries(control_plane PRIVATE Crow::Crow)

