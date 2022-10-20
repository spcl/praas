
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
  FetchContent_MakeAvailable(sockpp)
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
  FetchContent_MakeAvailable(spdlog)
else()
  add_custom_target(spdlog)
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
  set(SKIP_PERFORMANCE_COMPARISON)
  set(SKIP_PORTABILITY_TEST ON)
  set(JUST_INSTALL_CEREAL ON)
  FetchContent_MakeAvailable(cereal)
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
# google test
###
if(WITH_TESTING)
  message(STATUS "Downloading and building gtest")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

