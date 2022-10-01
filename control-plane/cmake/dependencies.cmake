
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

###
# stduuid
###
find_package(stduuid QUIET)
if(NOT stduuid_FOUND)
  message(STATUS "Downloading and building stduuid dependency")
  FetchContent_Declare(stduuid
    GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
  )
  FetchContent_Populate(stduuid)
  FetchContent_MakeAvailable(stduuid)
  add_subdirectory(${stduuid_SOURCE_DIR} ${stduuid_BINARY_DIR})
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
# redis
###
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)
find_package(redis QUIET)
if(NOT redis_FOUND)
  message(STATUS "Downloading and building redis-plus-plus dependency")
  FetchContent_Declare(redis
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG master
  )
  FetchContent_Populate(redis)
  FetchContent_MakeAvailable(redis)
  add_subdirectory(${redis_SOURCE_DIR} ${redis_BINARY_DIR})
else()
  add_custom_target(redis)
endif()

###
# crow
###
find_package(Crow QUIET)
if(NOT Crow_FOUND)
  message(STATUS "Downloading and building crow dependency")
  FetchContent_Declare(Crow
    GIT_REPOSITORY  https://github.com/CrowCpp/Crow.git
    #GIT_TAG         v0.3+3
  )
  #FetchContent_Populate(Crow)
  #set(CROW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
  #set(CROW_BUILD_TESTS  OFF CACHE INTERNAL "")
  #set(CROW_ENABLE_SSL ON CACHE INTERNAL "")
  #set(CROW_BUILD_EXAMPLES OFF)
  #set(CROW_BUILD_TESTS OFF)
  #set(CROW_ENABLE_SSL ON)
  #FetchContent_MakeAvailable(Crow)
  set(CROW_BUILD_EXAMPLES OFF)
  set(CROW_BUILD_TESTS  OFF)
  set(CROW_FEATURES "ssl")
  FetchContent_MakeAvailable(Crow)
endif()

