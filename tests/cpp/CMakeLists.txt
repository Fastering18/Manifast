add_executable(runtime_tests test_runtime.cpp)

target_link_libraries(runtime_tests PRIVATE manifast_core)

# We use simple C++ tests instead of GTest to keep dependencies minimal
add_test(NAME RuntimeTest COMMAND runtime_tests)
