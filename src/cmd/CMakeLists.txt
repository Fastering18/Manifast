add_executable(mifastc mifastc/main.cpp)

target_link_libraries(mifastc PRIVATE 
    manifast_jit 
    ${LLVM_AVAILABLE_LIBS}
    ${LLVM_SYSTEM_LIBS}
)

set_target_properties(mifastc PROPERTIES ENABLE_EXPORTS ON)

add_executable(mifast mifast/main.cpp)
target_link_libraries(mifast PRIVATE manifast_core manifast_jit fmt::fmt)
target_compile_definitions(mifast PRIVATE FMT_HEADER_ONLY)
set_target_properties(mifast PROPERTIES ENABLE_EXPORTS ON)
