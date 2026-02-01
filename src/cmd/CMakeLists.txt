add_executable(manifast
  main.cpp
)

target_link_libraries(manifast PRIVATE
  manifast_lib
  # argparse::argparse
)

set_target_properties(manifast PROPERTIES ENABLE_EXPORTS ON)
