add_library(manifast_lib
  Lexer.cpp
  Parser.cpp
)

# Allow including "manifast/..."
target_include_directories(manifast_lib PUBLIC 
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(manifast_lib PUBLIC
  # fmt::fmt
  # LLVMCore
  # LLVMOrcJIT
  # LLVMSupport
  # LLVMTarget
)
