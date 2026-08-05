# Manifast dependency discovery — portable, no machine-specific paths.
# Relies on CMAKE_PREFIX_PATH, LLVM_DIR, VCPKG toolchain, or system packages.

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
    core
    orcjit
    native
    lto
    passes
    mc
    object
    option
    ipo
    instrumentation
    bitreader
    bitwriter
    selectiondag
    support
  )

  message(STATUS "Found LLVM ${LLVM_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  include_directories(${LLVM_INCLUDE_DIRS})

  # LLVM_DEFINITIONS is often a single space-separated string; add_definitions() on
  # Windows turns it into one broken -D flag. Split first, then apply each define.
  if(LLVM_DEFINITIONS)
    separate_arguments(_manifast_llvm_defs NATIVE_COMMAND ${LLVM_DEFINITIONS})
    foreach(_def IN LISTS _manifast_llvm_defs)
      if(_def MATCHES "^-D(.*)")
        add_compile_definitions("${CMAKE_MATCH_1}")
      elseif(_def MATCHES "^-U(.*)")
        # strip undeffed macros if present
      elseif(NOT _def STREQUAL "")
        add_compile_definitions("${_def}")
      endif()
    endforeach()
  endif()

  # Platform system libs / optional stub targets for incomplete LLVM package configs
  include(${CMAKE_CURRENT_LIST_DIR}/Platform.cmake)
  manifast_setup_llvm_platform()
else()
  message(STATUS "LLVM disabled — building VM-only (embedded) mode")
endif()

if(MANIFAST_BUILD_TESTS)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    find_package(GTest QUIET)
  endif()
  if(GTest_FOUND)
    message(STATUS "Found GTest — C++ unit tests enabled")
    set(MANIFAST_HAS_GTEST ON)
  else()
    message(WARNING "GTest not found — GTest-based unit tests will be skipped. Install gtest or use vcpkg.")
    set(MANIFAST_HAS_GTEST OFF)
  endif()
endif()
