cmake_minimum_required(VERSION 3.21)

project(libstgx
  VERSION 0.1.0
  DESCRIPTION "State Transition Graph eXtended — build, edit, validate state graphs"
  LANGUAGES CXX
)

# Developer conveniences
if(PROJECT_IS_TOP_LEVEL)
  include(cmake/ProjectSettings.cmake)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STGX_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(STGX_BUILD_EXAMPLES "Build examples" OFF)
option(STGX_ENABLE_CLANG_TIDY "Enable clang-tidy on stgx targets" OFF)
set(STGX_CLANG_TIDY "" CACHE STRING "clang-tidy executable to use (name or full path)")

# --- dependencies

add_subdirectory(thirdparty)

# --- library

file(GLOB_RECURSE libstg_sources
  CONFIGURE_DEPENDS
  src/*.cpp
)

add_library(stgx ${libstg_sources})
add_library(stgx::stgx ALIAS stgx)

target_include_directories(stgx
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(stgx
  PRIVATE
    yaml-cpp::yaml-cpp
)

target_compile_features(stgx PUBLIC cxx_std_23)
target_compile_options(stgx PRIVATE -Wall -Wextra -Wpedantic)

# Carried on the target rather than picked up from ProjectSettings.cmake, so it
# also holds when a consumer links stgx into a shared library.
set_target_properties(stgx PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(STGX_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE
    NAMES
      ${STGX_CLANG_TIDY}
      clang-tidy-22 clang-tidy-21 clang-tidy-20 clang-tidy-19
      clang-tidy
    HINTS
      "$ENV{HOMEBREW_PREFIX}/opt/llvm/bin"
      /opt/homebrew/opt/llvm/bin
      /usr/local/opt/llvm/bin
    REQUIRED
  )
  set_target_properties(stgx PROPERTIES
    CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy"
  )
endif()


# --- examples

if(STGX_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()


# --- tests

if(STGX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
