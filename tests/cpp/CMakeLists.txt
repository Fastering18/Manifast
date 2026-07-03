cmake_minimum_required(VERSION 3.25)
find_package(GTest REQUIRED)

add_executable(test_bindings
    test_bindings.cpp
    ../../src/wasm/bindings.cpp
)

target_include_directories(test_bindings PRIVATE ../../include)

target_link_libraries(test_bindings PRIVATE
    GTest::gtest_main
    manifast_core
)

add_test(NAME test_bindings COMMAND test_bindings)
