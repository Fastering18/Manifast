cmake_minimum_required(VERSION 3.25)
project(manifast_wasm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Include directories from root
include_directories(../../include)

# Manifast Core Sources (Recompiled for Wasm)
# We recompile to ensure flags (like -fno-exceptions) match cleanly without linking errors
set(CORE_SOURCES
    ../../src/lib/Lexer.cpp
    ../../src/lib/Parser.cpp
    ../../src/lib/VM.cpp
    ../../src/lib/Compiler.cpp
    ../../src/lib/Runtime.cpp
    ../../src/lib/PlotBackend.cpp
)

add_executable(manifast bindings.cpp ${CORE_SOURCES})

# Compile Options
target_compile_options(manifast PRIVATE -fexceptions)

# Emscripten Link Options
target_link_options(manifast PRIVATE
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sALLOW_TABLE_GROWTH=1
    -sEXPORTED_RUNTIME_METHODS=["ccall","cwrap","UTF8ToString","addFunction","removeFunction"]
    -sEXPORTED_FUNCTIONS=["_mf_run_script","_mf_run_script_tier","_mf_get_plot_buffer","_mf_get_plot_buffer_size","_mf_set_event_callback","_malloc","_free"]
    -sDISABLE_EXCEPTION_CATCHING=0
    -sMODULARIZE=1
    -sEXPORT_NAME="ManifastModule"
    -sNO_EXIT_RUNTIME=1
)

# Move outputs to /docs automatically
add_custom_command(TARGET manifast POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/../../docs
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:manifast> ${CMAKE_SOURCE_DIR}/../../docs/manifast.js
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/manifast.wasm ${CMAKE_SOURCE_DIR}/../../docs/manifast.wasm
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_SOURCE_DIR}/../../docs/index.html
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/robots.txt ${CMAKE_SOURCE_DIR}/../../docs/robots.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/llms.txt ${CMAKE_SOURCE_DIR}/../../docs/llms.txt
    COMMENT "Copying WebAssembly outputs to /docs directory"
)
