
cmake_minimum_required(VERSION 3.11)
project(praas-serving)

file(GLOB sdk_files "src/*.cpp")
add_executable(praas_serving ${sdk_files})
target_include_directories(praas_serving SYSTEM PUBLIC "include")
set_target_properties(praas_serving PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)

target_include_directories(praas_serving SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(praas_serving SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(praas_serving SYSTEM PRIVATE $<TARGET_PROPERTY:Drogon::Drogon,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(praas_serving PRIVATE common_library)
target_link_libraries(praas_serving PRIVATE spdlog::spdlog)
target_link_libraries(praas_serving PRIVATE Drogon::Drogon)
target_link_libraries(praas_serving PRIVATE TBB::tbb)

