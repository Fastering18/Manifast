add_executable(mifastc mifastc/main.cpp)
target_link_libraries(mifastc PRIVATE manifast_jit)
set_target_properties(mifastc PROPERTIES ENABLE_EXPORTS ON)

add_executable(mifast mifast/main.cpp)
target_link_libraries(mifast PRIVATE manifast_jit fmt::fmt)
set_target_properties(mifast PROPERTIES ENABLE_EXPORTS ON)
