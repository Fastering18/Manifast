cmake_minimum_required(VERSION 3.25)

add_executable(manifast_tests
  test_parser.cpp
)

target_link_libraries(manifast_tests PRIVATE manifast_core)

enable_testing()
add_test(NAME ParserTests COMMAND manifast_tests)
