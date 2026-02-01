cmake_minimum_required(VERSION 3.25)

project(manifast
  VERSION 0.1.0
  LANGUAGES CXX C
)

# --- Options ---
option(MANIFAST_BUILD_TESTS "Build tests" ON)

# --- Standard Setup ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Enforce standards

# --- Dependencies (vcpkg) ---
find_package(fmt CONFIG REQUIRED)
find_package(LLVM REQUIRED CONFIG COMPONENTS core orcjit native)

message(STATUS "Found LLVM ${LLVM_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Map LLVM includes and libs
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# --- Output Directories ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Subdirectories ---
add_subdirectory(src/lib)
add_subdirectory(src/cmd)

if(MANIFAST_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
