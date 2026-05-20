cmake_minimum_required(VERSION 3.20)
project(optix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPTIX_BUILD_TESTS "Build tests" ON)
option(OPTIX_ENABLE_LIBTOOLING "Enable Clang LibTooling based indexer" ON)

if(NOT OPTIX_ENABLE_LIBTOOLING)
  message(FATAL_ERROR "OPTIX_ENABLE_LIBTOOLING must be ON. Optix requires LibTooling in strict mode.")
endif()

# 自动探测 Clang_DIR，优先兼容 Homebrew LLVM（例如 /opt/homebrew/Cellar/llvm@17/*）。
if(OPTIX_ENABLE_LIBTOOLING)
  if(NOT DEFINED Clang_DIR OR Clang_DIR STREQUAL "" OR Clang_DIR MATCHES "NOTFOUND")
    set(_optix_clang_dir_candidates
        "/opt/homebrew/opt/llvm/lib/cmake/clang"
        "/usr/local/opt/llvm/lib/cmake/clang")

    file(GLOB _optix_llvm_cellar_dirs "/opt/homebrew/Cellar/llvm@*/*")
    list(SORT _optix_llvm_cellar_dirs)
    list(REVERSE _optix_llvm_cellar_dirs)
    foreach(_dir IN LISTS _optix_llvm_cellar_dirs)
      list(APPEND _optix_clang_dir_candidates "${_dir}/lib/cmake/clang")
    endforeach()

    foreach(_clang_dir IN LISTS _optix_clang_dir_candidates)
      if(EXISTS "${_clang_dir}/ClangConfig.cmake")
        set(Clang_DIR "${_clang_dir}" CACHE PATH "Path to ClangConfig.cmake" FORCE)
        message(STATUS "Detected Clang_DIR=${Clang_DIR}")
        break()
      endif()
    endforeach()
  endif()
endif()

if(UNIX)
  add_compile_definitions(OPTIX_PLATFORM_POSIX)
endif()

add_subdirectory(lib)
add_subdirectory(tools)

if(OPTIX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
