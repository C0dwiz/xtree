add_test(
  NAME cli_test
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.sh $<TARGET_FILE:xtree>
)
set_tests_properties(cli_test PROPERTIES TIMEOUT 30)
