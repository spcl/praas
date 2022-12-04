
set(TESTS unit/messages.cpp
          unit/config.cpp
)
foreach(test ${TESTS})

  PraaS_AddTest("process" test_name ${test})
  process_dependencies(${test_name})
  target_link_libraries(${test_name} PRIVATE controller_lib)
  process_link(${test_name})

endforeach()

###
# Integration tests
###

set(TESTS integration/invocation.cpp
          integration/invocation_many_workers.cpp
          integration/get_put.cpp
          integration/local_invocation.cpp
)
foreach(test ${TESTS})

  PraaS_AddTest("process" test_name ${test})
  process_dependencies(${test_name})
  target_link_libraries(${test_name} PRIVATE controller_lib)
  process_link(${test_name})

endforeach()

###
# C libraries
###
add_library(example_cpp SHARED integration/examples/cpp/test.cpp)
target_include_directories(example_cpp PUBLIC "tests/integration/examples/cpp/")
set_target_properties(example_cpp PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples)
target_link_libraries(example_cpp PRIVATE function_lib)

set(PRAAS_DIRECTORY ${CMAKE_BINARY_DIR})
set(PRAAS_SOURCE_DIRECTORY ${CMAKE_SOURCE_DIR})
configure_file(integration/configuration.json.in integration/configuration.json @ONLY)

