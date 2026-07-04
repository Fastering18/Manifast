add_executable(runtime_tests test_runtime.cpp)

target_link_libraries(runtime_tests PRIVATE manifast_core)

# We use simple C++ tests instead of GTest to keep dependencies minimal
add_test(NAME RuntimeTest COMMAND runtime_tests)

add_executable(test_wasm_print_any test_wasm_print_any.cpp)
target_link_libraries(test_wasm_print_any PRIVATE manifast_core)
add_test(NAME WasmPrintAnyTest COMMAND test_wasm_print_any)
