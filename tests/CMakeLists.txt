add_executable(manifast_tests
  TestMain.cpp
)

# If GTest is not found, we might skip or use basic assertions
# For now, let's just make it compilable
target_link_libraries(manifast_tests PRIVATE
  manifast_lib
)
