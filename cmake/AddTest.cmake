
function(PraaS_AddTest target_name test_file)

  if(NOT WITH_TESTING)
    message(FATAL_ERROR "Tests are disabled - cannot add tests")
  endif()

  string(REPLACE "." "_" name ${test_file})
  string(REPLACE "/" "_" name ${test_file})
  add_executable(${name} ${test_file})

  target_link_libraries(${name} PRIVATE GTest::gtest_main)
  target_link_libraries(${name} PRIVATE GTest::gmock_main)

  gtest_discover_tests(${name})

  set(${target_name} ${name} PARENT_SCOPE)

endfunction()
