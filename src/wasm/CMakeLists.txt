cmake_minimum_required(VERSION 3.25)
project(manifast_wasm)

set(CMAKE_CXX_STANDARD 20)

# Include directories from root
include_directories(../../include)

# Manifast Core Sources (Recompiled for Wasm)
# We recompile to ensure flags (like -fno-exceptions) match cleanly without linking errors
set(CORE_SOURCES
    ../../src/lib/Lexer.cpp
    ../../src/lib/Parser.cpp
    ../../src/lib/VM.cpp
    ../../src/lib/Compiler.cpp
    # Runtime.cpp might not be needed if we override runtime functions in bindings, 
    # but Runtime types (Any) are used.
    # Note: Runtime.cpp implementation of standard library functions might conflict if we want custom JS overrides.
    # But headers are needed.
)

add_executable(manifast bindings.cpp ${CORE_SOURCES})

# Compile Options
target_compile_options(manifast PRIVATE -fno-exceptions)

# Emscripten Link Options
target_link_options(manifast PRIVATE
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sEXPORTED_RUNTIME_METHODS=["ccall","cwrap","UTF8ToString"]
    -sEXPORTED_FUNCTIONS=["_mf_run_script","_malloc","_free"]
    -sNO_EXIT_RUNTIME=1
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/index.html
)
