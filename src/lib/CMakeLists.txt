add_library(manifast_lib
  Lexer.cpp
  Parser.cpp
  CodeGen.cpp
  Runtime.cpp
)

# Allow including "manifast/..."
target_include_directories(manifast_lib PUBLIC 
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

target_link_libraries(manifast_lib PUBLIC
  fmt::fmt
  ${llvm_libs}
)
