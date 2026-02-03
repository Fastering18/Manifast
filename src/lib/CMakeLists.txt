add_library(manifast_core
  Lexer.cpp
  Parser.cpp
  Runtime.cpp
  VM.cpp
  Compiler.cpp
)

target_include_directories(manifast_core PUBLIC 
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(manifast_core PUBLIC
  fmt::fmt
)

# --- JIT Library (LLVM) ---
add_library(manifast_jit
  CodeGen.cpp
)

target_include_directories(manifast_jit PUBLIC 
  ${CMAKE_SOURCE_DIR}/include
)

llvm_map_components_to_libnames(llvm_libs
  core
  orcjit
  support
  target
  executionengine
  analysis
  native
)

target_link_libraries(manifast_jit PUBLIC
  manifast_core
  ${llvm_libs}
)

# Alias for backward compatibility if needed, or just remove
add_library(manifast_lib ALIAS manifast_jit)
