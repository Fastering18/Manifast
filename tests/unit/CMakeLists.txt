add_executable(parser_error_tests
  ParserErrorTest.cpp
)

target_link_libraries(parser_error_tests PRIVATE
  manifast_core
  GTest::gtest
  GTest::gtest_main
)

add_test(NAME parser_error_tests COMMAND parser_error_tests)