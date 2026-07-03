#include <iostream>
#include <cstdlib>
#include "manifast/Runtime.h"

// Define a simple mock callback
static bool g_callback_called = false;
void mock_clear_output_callback() {
    g_callback_called = true;
}

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Test failed: " << message << std::endl; \
            std::exit(1); \
        } \
    } while (false)

void test_manifast_set_clear_output_callback() {
    // Set the callback
    manifast_set_clear_output_callback(mock_clear_output_callback);

    // Create os module using the API. (Notice: manifast_impor is the correct Indonesian spelling in this codebase)
    Any* os_module = manifast_impor("os");
    ASSERT_TRUE(os_module != nullptr, "os module should be importable");
    ASSERT_TRUE(os_module->type == ANY_OBJECT, "os module should be an object");

    // Get clearOutput function
    Any* clear_func = manifast_object_get(os_module, "clearOutput");
    ASSERT_TRUE(clear_func != nullptr, "clearOutput should exist in os module");
    ASSERT_TRUE(clear_func->type == ANY_NATIVE, "clearOutput should be a native function");

    // We use manifast_call_dynamic to safely call the function.
    // manifast_call_dynamic handles the stack and arguments properly.
    g_callback_called = false;
    manifast_call_dynamic(clear_func, nullptr, 0);

    // Verify it was called
    ASSERT_TRUE(g_callback_called, "The clear output callback should have been called");

    // Reset to null to avoid side effects
    manifast_set_clear_output_callback(nullptr);

    std::cout << "test_manifast_set_clear_output_callback passed!" << std::endl;
}

#ifndef RUNTIME_TEST_NO_MAIN
int main() {
    std::cout << "Running C++ Tests..." << std::endl;

    test_manifast_set_clear_output_callback();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
#endif
