
function(PraaS_AddTest component target_name test_file add_to_gtest)

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

  # FIXME: the python path should only be set for a subset of tests
  if(add_to_gtest)
    gtest_discover_tests(
      ${name}
      TEST_PREFIX "${component}:"
      PROPERTIES ENVIRONMENT_MODIFICATION
      "PYTHONPATH=path_list_append:${CMAKE_BINARY_DIR}/process"
    )
  endif()

  set(${target_name} ${name} PARENT_SCOPE)

endfunction()
