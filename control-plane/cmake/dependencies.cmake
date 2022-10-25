
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

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
  FetchContent_MakeAvailable(redis)
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
  set(CROW_BUILD_EXAMPLES OFF)
  set(CROW_BUILD_TESTS  OFF)
  set(CROW_FEATURES "ssl")
  FetchContent_MakeAvailable(Crow)
endif()

