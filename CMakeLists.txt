cmake_minimum_required(VERSION 3.25)

project(manifast
  VERSION 0.0.13
  LANGUAGES CXX C
)

# --- Options ---
option(MANIFAST_ENABLE_LLVM "Enable LLVM JIT/AOT support" ON)

# --- Standard Setup ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies (vcpkg) ---
find_package(fmt CONFIG REQUIRED)
find_package(asmjit CONFIG QUIET)
if(asmjit_FOUND)
  message(STATUS "Found AsmJit — lightweight JIT enabled")
  set(MANIFAST_HAS_ASMJIT ON)
else()
  message(STATUS "AsmJit not found — building without lightweight JIT")
  set(MANIFAST_HAS_ASMJIT OFF)
endif()

if(MANIFAST_ENABLE_LLVM)
  find_package(LLVM REQUIRED CONFIG COMPONENTS 
      core orcjit native lto passes mc object option 
      ipo instrumentation bitreader bitwriter selectiondag 
      support
  )

  message(STATUS "Found LLVM ${LLVM_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  include_directories(${LLVM_INCLUDE_DIRS})
  add_definitions(${LLVM_DEFINITIONS})

  if(WIN32)
    set(LLVM_SYSTEM_LIBS psapi shell32 ole32 uuid advapi32 ws2_32 ntdll z)

    if(TARGET LLVMWindowsManifest)
      set_target_properties(LLVMWindowsManifest PROPERTIES INTERFACE_INCLUDE_DIRECTORIES 
        "D:/Program/msys64/ucrt64/include/libxml2;D:/Program/msys64/ucrt64/include")
    endif()

    if(TARGET LibXml2::LibXml2)
      set_target_properties(LibXml2::LibXml2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES 
        "D:/Program/msys64/ucrt64/include/libxml2;D:/Program/msys64/ucrt64/include")
    endif()
  else()
    if(NOT TARGET CURL::libcurl)
      add_library(CURL::libcurl INTERFACE IMPORTED)
    endif()
    if(NOT TARGET LibEdit::LibEdit)
      add_library(LibEdit::LibEdit INTERFACE IMPORTED)
    endif()
  endif()
else()
  message(STATUS "LLVM disabled — building VM-only (embedded) mode")
endif()


# --- Output Directories ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Subdirectories ---
add_subdirectory(src/lib)
add_subdirectory(src/cmd)


# --- CPack Configuration ---
set(CPACK_PACKAGE_NAME "manifast")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Fastering18")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast Indonesian-syntax programming language powered by LLVM")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Fastering18/Manifast")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/README.md")

# Install targets
install(TARGETS mifast RUNTIME DESTINATION bin)
if(MANIFAST_ENABLE_LLVM)
  install(TARGETS mifastc RUNTIME DESTINATION bin)
endif()
install(DIRECTORY include/manifast DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "vendor" EXCLUDE
)
install(TARGETS manifast_core ARCHIVE DESTINATION lib)
if(MANIFAST_ENABLE_LLVM)
  install(TARGETS manifast_jit ARCHIVE DESTINATION lib)
endif()

# NSIS (Windows installer)
set(CPACK_NSIS_DISPLAY_NAME "Manifast Programming Language")
set(CPACK_NSIS_PACKAGE_NAME "Manifast")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# DEB (Debian package)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Fastering18 <fastering18@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libstdc++6")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Fastering18/Manifast")

include(CPack)
