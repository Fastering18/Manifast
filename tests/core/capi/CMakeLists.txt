

add_executable(test_c_api test_array_push.cpp)
target_link_libraries(test_c_api PRIVATE manifast_core GTest::gtest_main)

add_test(NAME C_API_Tests COMMAND test_c_api)
