
function(PraaS_AddTest component target_name test_file)

  if(NOT PRAAS_WITH_TESTING)
    message(FATAL_ERROR "Tests are disabled - cannot add tests")
  endif()

  string(REPLACE "." "_" name ${component}_${test_file})
  string(REPLACE "/" "_" name ${name})
  string(REPLACE "-" "_" name ${name})
  string(REPLACE "_cpp" "" name ${name})
  add_executable(${name} ${test_file})

  target_link_libraries(${name} PRIVATE GTest::gtest_main)
  target_link_libraries(${name} PRIVATE GTest::gmock_main)

  gtest_discover_tests(${name} TEST_PREFIX "${component}:")

  set(${target_name} ${name} PARENT_SCOPE)

endfunction()
