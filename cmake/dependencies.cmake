
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
  FetchContent_Populate(cxxopts)
  FetchContent_MakeAvailable(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR})
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
  FetchContent_Populate(sockpp)
  FetchContent_MakeAvailable(sockpp)
  set(SOCKPP_BUILD_SHARED OFF CACHE INTERNAL "Build SHARED libraries")
  set(SOCKPP_BUILD_STATIC ON CACHE INTERNAL "Build SHARED libraries")
  add_subdirectory(${sockpp_SOURCE_DIR} ${sockpp_BINARY_DIR})
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
  FetchContent_Populate(spdlog)
  FetchContent_MakeAvailable(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
else()
  add_custom_target(spdlog)
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
FetchContent_GetProperties(threadpool)
if(NOT threadpool_POPULATED)
  FetchContent_Populate(threadpool)
endif()

###
# google test
###
if(${WITH_TESTING})
  message(STATUS "Downloading and building gtest")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

