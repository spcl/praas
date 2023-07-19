
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

include(FetchContent)

###
# cxxopts
###
find_package(cxxopts QUIET)
if(NOT cxxopts_FOUND)
  message(STATUS "Downloading and building cxxopts dependency")
  FetchContent_Declare(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    CMAKE_ARGS -DCXXOPTS_BUILD_EXAMPLES=Off -DCXXOPTS_BUILD_TESTS=Off
  )
  FetchContent_MakeAvailable(cxxopts)
else()
  message(STATUS "Found cxxopts dependency")
endif()

###
# sockpp
###
find_package(sockpp QUIET)
if(NOT sockpp_FOUND)
  message(STATUS "Downloading and building sockpp dependency")
  FetchContent_Declare(sockpp
    GIT_REPOSITORY https://github.com/spcl/sockpp
  )
  set(SOCKPP_BUILD_SHARED OFF CACHE INTERNAL "Build SHARED libraries")
  set(SOCKPP_BUILD_STATIC ON CACHE INTERNAL "Build SHARED libraries")
  FetchContent_GetProperties(sockpp)
  if(NOT sockpp_POPULATED)
    FetchContent_Populate(sockpp)
    add_subdirectory(${sockpp_SOURCE_DIR} ${sockpp_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
else()
  message(STATUS "Found sockpp dependency")
endif()

###
# format
###
find_package(fmt QUIET)
if(NOT fmt_FOUND)
  message(STATUS "Downloading and building fmt dependency")
  FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
  )
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(fmt)
else()
  message(STATUS "Found fmt dependency")
endif()

###
# spdlog
###
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "Downloading and building spdlog dependency")
  FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.8.0
  )
  SET(SPDLOG_FMT_EXTERNAL OFF)
  FetchContent_MakeAvailable(spdlog)
  set_property(TARGET spdlog PROPERTY POSITION_INDEPENDENT_CODE ON)
else()
  add_custom_target(spdlog)
  message(STATUS "Found spdlog dependency")
endif()

###
# cereal
###
find_package(cereal QUIET)
if(NOT cereal_FOUND)
  message(STATUS "Downloading and building cereal dependency")
  FetchContent_Declare(cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  )
  set(SKIP_PERFORMANCE_COMPARISON ON)
  set(SKIP_PORTABILITY_TEST ON)
  set(JUST_INSTALL_CEREAL ON)
  FetchContent_MakeAvailable(cereal)
  add_library(cereal::cereal ALIAS cereal)
else()
  message(STATUS "Found cereal dependency")
endif()

###
# stduuid
###
find_package(stduuid QUIET)
if(NOT stduuid_FOUND)
  message(STATUS "Downloading and building stduuid dependency")
  FetchContent_Declare(stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
  )
  # disable installing gsl
  FetchContent_GetProperties(stduuid)
  if(NOT stduuid_POPULATED)
    FetchContent_Populate(stduuid)
    add_subdirectory(${stduuid_SOURCE_DIR} ${stduuid_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
endif()

###
# threadpool
###
FetchContent_Declare(threadpool
  GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
  GIT_TAG master
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
)
FetchContent_MakeAvailable(threadpool)

###
# TBB - static libraries are discouraged
###
find_package(TBB QUIET)
if(NOT TBB_FOUND)
  message(STATUS "Downloading and building TBB dependency")
  FetchContent_Declare(TBB
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
  )
  set(TBB_TEST OFF CACHE INTERNAL "")
  set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
  FetchContent_MakeAvailable(TBB)
endif()

###
# drogon
###
find_package(Drogon QUIET)
if(NOT Drogon_FOUND)
  message(STATUS "Downloading and building drogon dependency")
  FetchContent_Declare(Drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon
  )
  set (BUILD_TESTING OFF CACHE INTERNAL "Turn off tests")
  set (BUILD_SQLITE OFF CACHE INTERNAL "Turn off tests")
  set (BUILD_POSTGRESQL OFF CACHE INTERNAL "Turn off tests")
  set (BUILD_MYSQL OFF CACHE INTERNAL "Turn off tests")
  set (BUILD_ORM OFF CACHE INTERNAL "Turn off tests")
  set (BUILD_BROTLI OFF CACHE INTERNAL "Turn off tests")
  FetchContent_GetProperties(Drogon)
  if(NOT drogon_POPULATED)
    FetchContent_Populate(Drogon)
    add_subdirectory(${drogon_SOURCE_DIR} ${drogon_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
  add_library(Drogon::Drogon ALIAS drogon)
else()
  message(STATUS "Found drogon dependency")
endif()

if(PRAAS_WITH_TESTING)

  ###
  # google test
  ###
  message(STATUS "Downloading and building gtest")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  set(BUILD_GMOCK ON)
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()

endif()

