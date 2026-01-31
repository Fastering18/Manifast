find_package(GTest CONFIG REQUIRED)

add_executable(manifast_tests
  TestMain.cpp
)

target_link_libraries(manifast_tests PRIVATE
  manifast_lib
  GTest::gtest
  GTest::gtest_main
)

# Standard GTest discover
include(GoogleTest)
gtest_discover_tests(manifast_tests)
