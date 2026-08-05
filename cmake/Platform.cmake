# Platform helpers for LLVM and system libraries.
# No hardcoded user drive letters.

function(manifast_setup_llvm_platform)
  if(WIN32)
    # Common Windows system libs linked by LLVM static libraries
    set(LLVM_SYSTEM_LIBS
      psapi
      shell32
      ole32
      uuid
      advapi32
      ws2_32
      ntdll
      z
      PARENT_SCOPE
    )

    # Prefer include paths already on CMAKE_PREFIX_PATH / MSYS2 prefix
    # rather than absolute machine-specific directories.
    set(_manifast_prefix_candidates "")
    if(DEFINED ENV{MSYSTEM_PREFIX})
      list(APPEND _manifast_prefix_candidates "$ENV{MSYSTEM_PREFIX}")
    endif()
    if(DEFINED ENV{MSYS2_ROOT})
      list(APPEND _manifast_prefix_candidates
        "$ENV{MSYS2_ROOT}/ucrt64"
        "$ENV{MSYS2_ROOT}/mingw64"
      )
    endif()
    foreach(_p IN ITEMS
        "C:/msys64/ucrt64"
        "C:/msys64/mingw64"
        "D:/msys64/ucrt64"
        "D:/msys64/mingw64"
    )
      if(EXISTS "${_p}")
        list(APPEND _manifast_prefix_candidates "${_p}")
      endif()
    endforeach()

    set(_libxml_includes "")
    foreach(_prefix IN LISTS _manifast_prefix_candidates)
      if(EXISTS "${_prefix}/include/libxml2")
        list(APPEND _libxml_includes
          "${_prefix}/include/libxml2"
          "${_prefix}/include"
        )
        break()
      endif()
    endforeach()

    if(_libxml_includes)
      if(TARGET LLVMWindowsManifest)
        set_target_properties(LLVMWindowsManifest PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${_libxml_includes}"
        )
      endif()
      if(TARGET LibXml2::LibXml2)
        set_target_properties(LibXml2::LibXml2 PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${_libxml_includes}"
        )
      endif()
    endif()
  else()
    # Some distro LLVM configs reference optional targets that may be missing
    if(NOT TARGET CURL::libcurl)
      add_library(CURL::libcurl INTERFACE IMPORTED)
    endif()
    if(NOT TARGET LibEdit::LibEdit)
      add_library(LibEdit::LibEdit INTERFACE IMPORTED)
    endif()
    set(LLVM_SYSTEM_LIBS "" PARENT_SCOPE)
  endif()
endfunction()
