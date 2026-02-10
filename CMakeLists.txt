cmake_minimum_required(VERSION 3.25)

project(manifast
  VERSION 0.0.12
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
find_package(LLVM REQUIRED CONFIG COMPONENTS 
    core orcjit native lto passes mc object option 
    ipo instrumentation bitreader bitwriter selectiondag 
    support
)

message(STATUS "Found LLVM ${LLVM_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# System libs needed for static LLVM/LLD linking
set(LLVM_SYSTEM_LIBS psapi shell32 ole32 uuid advapi32 ws2_32 ntdll z zstd)

# Map LLVM includes and libs
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Workaround for faulty libxml2 path in some MSYS2 LLVM packages
# Force set correct absolute paths to avoid "non-existent path" errors
if(TARGET LLVMWindowsManifest)
  set_target_properties(LLVMWindowsManifest PROPERTIES INTERFACE_INCLUDE_DIRECTORIES 
    "D:/Program/msys64/ucrt64/include/libxml2;D:/Program/msys64/ucrt64/include")
endif()

if(TARGET LibXml2::LibXml2)
  set_target_properties(LibXml2::LibXml2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES 
    "D:/Program/msys64/ucrt64/include/libxml2;D:/Program/msys64/ucrt64/include")
endif()


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
