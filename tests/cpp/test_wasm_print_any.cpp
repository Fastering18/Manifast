#include <iostream>
#include "manifast/Runtime.h"
#include <string>
#include <cstring>
#include <cassert>

// Avoid duplicate definition clash with Runtime.cpp
#define manifast_print_any dummy_manifast_print_any
#define manifast_println_any dummy_manifast_println_any
#define manifast_assert dummy_manifast_assert

#define __EMSCRIPTEN__
#include "../../src/wasm/bindings.cpp"
#undef __EMSCRIPTEN__

#undef manifast_print_any
#undef manifast_println_any
#undef manifast_assert

int main() {
    g_wasm_output = "";

    // Create a deeply nested array
    // Since we need to go deeper than 32, we create 34 levels
    const int depth = 34;
    ::Any arrays[depth];
    ::ManifastArray arrStructs[depth];

    for (int i = 0; i < depth; i++) {
        arrStructs[i].size = (i < depth - 1) ? 1 : 0;
        arrStructs[i].capacity = 1;
        arrStructs[i].elements = (i < depth - 1) ? &arrays[i+1] : nullptr;

        arrays[i].type = 6; // ANY_ARRAY
        arrays[i].ptr = &arrStructs[i];
    }

    wasm_print_any(&arrays[0]);

    // We expect the output to contain "..." because of the depth limit
    if (g_wasm_output.find("...") == std::string::npos) {
        std::cerr << "Test failed: Expected '...' in output due to depth limit, but got: " << g_wasm_output << std::endl;
        return 1;
    }

    std::cout << "test_wasm_print_any passed" << std::endl;
    return 0;
}
