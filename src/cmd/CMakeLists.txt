add_executable(manifast
  main.cpp
)

target_link_libraries(manifast PRIVATE
  manifast_lib
  # argparse::argparse
)
