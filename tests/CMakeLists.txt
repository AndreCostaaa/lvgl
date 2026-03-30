if(ESP_PLATFORM)
  return()
endif()

cmake_minimum_required(VERSION 3.16)
project(lvgl_tests LANGUAGES C CXX)

if(WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows")
  include($ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)

# =============================================================================
# Compiler flag filtering
# =============================================================================
set(FLAG_CHECK_WHITELIST --coverage -fsanitize=address -Werror)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
function(filter_compiler_options lang options_out)
  set(options ${ARGN})
  foreach(option ${options})
    if(option IN_LIST FLAG_CHECK_WHITELIST)
      list(APPEND ${options_out} ${option})
      continue()
    endif()

    string(TOUPPER FLAG_SUPPORTED_FOR_${lang}_${option} option_var_name)
    string(REPLACE "-" "_" option_var_name ${option_var_name})
    string(REPLACE "+" "_" option_var_name ${option_var_name})

    if(${lang} STREQUAL C)
      check_c_compiler_flag(${option} ${option_var_name})
    elseif(${lang} STREQUAL CXX)
      check_cxx_compiler_flag(${option} ${option_var_name})
    else()
      message(FATAL_ERROR "Unknown language ${lang}")
    endif()
    if(${option_var_name})
      list(APPEND ${options_out} ${option})
    endif()
  endforeach()
  set(${options_out}
      ${${options_out}}
      PARENT_SCOPE)
endfunction()

# =============================================================================
# Valgrind / memcheck
# =============================================================================
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
  set(MEMORYCHECK_COMMAND ${VALGRIND_EXECUTABLE})
  set(MEMORYCHECK_COMMAND_OPTIONS --error-exitcode=1)
endif()

include(CTest)

# =============================================================================
# Build configuration
#
# Choose exactly one of:
#
# -DBUILD_CONFIG=SW_SYSHEAP Full SW rendering, system heap + ASan
# -DBUILD_CONFIG=SW_DEFHEAP Full SW rendering, builtin heap + ASan
# -DBUILD_CONFIG=VG_LITE    VG-Lite GPU (ThorVG simulation), system heap + ASan
# -DBUILD_CONFIG=RISCV_V    RISC-V Vector SW rendering, system heap + ASan
#
# Build-only configs (compile check, no tests executed):
# -DBUILD_CONFIG=COLOR_DEPTH_1   1-bit monochrome, minimal config
# -DBUILD_CONFIG=COLOR_DEPTH_8   8-bit, minimal config
# -DBUILD_CONFIG=COLOR_DEPTH_16  16-bit, minimal config
# -DBUILD_CONFIG=COLOR_DEPTH_24  24-bit, no extras -DBUILD_CONFIG=SDL SDL
# display backend
# =============================================================================

set(BUILD_CONFIG
    ""
    CACHE
      STRING
      "Test build configuration. One of: SW_SYSHEAP SW_DEFHEAP VG_LITE RISCV_V")

if(NOT BUILD_CONFIG)
  message(FATAL_ERROR "Must set -DBUILD_CONFIG=<value>. "
                      "Valid values: SW_SYSHEAP SW_DEFHEAP VG_LITE RISCV_V")
endif()

set(SANITIZE_AND_COVERAGE_OPTIONS -fsanitize=address -fsanitize=leak
                                  -fsanitize=undefined --coverage)

# --- Config: SW_SYSHEAP ---
if(BUILD_CONFIG STREQUAL "SW_SYSHEAP")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=5 LVGL_CI_USING_SYS_HEAP)
  set(SANITIZER_OPTIONS ${SANITIZE_AND_COVERAGE_OPTIONS})
  set(ENABLE_TESTS ON)

  # --- Config: SW_DEFHEAP ---
elseif(BUILD_CONFIG STREQUAL "SW_DEFHEAP")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=5 LV_USE_OBJ_PROPERTY=1
                        LV_USE_OBJ_PROPERTY_NAME=1 LVGL_CI_USING_DEF_HEAP)
  set(SANITIZER_OPTIONS ${SANITIZE_AND_COVERAGE_OPTIONS})
  set(ENABLE_TESTS ON)

  # --- Config: SW_MEMCHECK (valgrind, no sanitizer) ---
elseif(BUILD_CONFIG STREQUAL "SW_MEMCHECK")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=5 LVGL_CI_USING_SYS_HEAP)
  set(SANITIZER_OPTIONS "") # sanitizer conflicts with valgrind's LD_PRELOAD
  set(ENABLE_TESTS ON)

  # --- Config: VG_LITE ---
elseif(BUILD_CONFIG STREQUAL "VG_LITE")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=6 LVGL_CI_USING_SYS_HEAP)
  set(EXTRA_COMPILE_OPTIONS -Wno-dangling-pointer) # ThorVG workaround
  set(SANITIZER_OPTIONS ${SANITIZE_AND_COVERAGE_OPTIONS})
  set(ENABLE_TESTS ON)
  set(IS_VG_LITE ON)

  if($ENV{NON_AMD64_BUILD})
    list(APPEND LVGL_COMPILE_DEFS REF_IMG_TOLERANCE=9)
  endif()

  # --- Config: RISCV_V ---
elseif(BUILD_CONFIG STREQUAL "RISCV_V")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=5 LVGL_CI_USING_SYS_HEAP
                        LV_USE_DRAW_SW_ASM=LV_DRAW_SW_ASM_RISCV_V)
  set(SANITIZER_OPTIONS ${SANITIZE_AND_COVERAGE_OPTIONS})
  set(ENABLE_TESTS ON)
  message(STATUS "RISC-V Vector (RVV) software emulation test enabled")

  # --- Build-only configs (compile smoke tests, no ENABLE_TESTS) ---
elseif(BUILD_CONFIG STREQUAL "COLOR_DEPTH_1")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=1)
elseif(BUILD_CONFIG STREQUAL "COLOR_DEPTH_8")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=2)
elseif(BUILD_CONFIG STREQUAL "COLOR_DEPTH_16")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=3)
elseif(BUILD_CONFIG STREQUAL "COLOR_DEPTH_24")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=4)
elseif(BUILD_CONFIG STREQUAL "SDL")
  set(LVGL_COMPILE_DEFS LV_TEST_OPTION=7)

else()
  message(FATAL_ERROR "Unknown BUILD_CONFIG value: '${BUILD_CONFIG}'")
endif()

# =============================================================================
# Non-AMD64 / 32-bit cross-build
# =============================================================================
if($ENV{NON_AMD64_BUILD})
  message(STATUS "Non-AMD64 target: enabling 32-bit build flags")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -m32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}    -m32")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")

  set(CMAKE_LIBRARY_PATH
      "/usr/lib/i386-linux-gnu"
      CACHE PATH "32-bit lib search path")
  set(ENV{PKG_CONFIG_PATH}
      "/usr/lib/i386-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")

  list(APPEND LVGL_COMPILE_DEFS NON_AMD64_BUILD)
endif()

# =============================================================================
# Warning flags
# =============================================================================
if(NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC"))
  set(LVGL_WARNING_FLAGS
      -pedantic-errors
      -Wall
      -Wextra
      -Werror
      -Wformat-security
      -Wclobbered
      -Wdeprecated
      -Wdouble-promotion
      -Wempty-body
      -Wmaybe-uninitialized
      -Wmissing-prototypes
      -Wmultichar
      -Wpedantic
      -Wpointer-arith
      -Wreturn-type
      -Wshadow
      -Wshift-negative-value
      -Wsizeof-pointer-memaccess
      -Wtype-limits
      -Wundef
      -Wuninitialized
      -Wunreachable-code
      -Wunused-function
      -Werror=float-conversion
      -Werror=strict-aliasing
      -Wno-double-promotion # re-suppressed after enabling above
      -Wno-unreachable-code
      -Wno-gnu-zero-variadic-macro-arguments
      -Wno-overlength-strings)
else()
  set(LVGL_WARNING_FLAGS /W3 /we4013)
endif()

# SW_DEFHEAP enables LV_USE_OBJ_PROPERTY which uses C11 anonymous structs;
# pedantic mode rejects these.
if(BUILD_CONFIG STREQUAL "SW_DEFHEAP")
  list(REMOVE_ITEM LVGL_WARNING_FLAGS -pedantic-errors -Wpedantic)
endif()

set(COMPILE_OPTIONS ${LVGL_WARNING_FLAGS} ${EXTRA_COMPILE_OPTIONS})
foreach(def ${LVGL_COMPILE_DEFS})
  list(APPEND COMPILE_OPTIONS -D${def})
endforeach()
list(APPEND COMPILE_OPTIONS ${SANITIZER_OPTIONS})

filter_compiler_options(C LVGL_C_COMPILE_OPTIONS ${COMPILE_OPTIONS})

if(NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC"))
  filter_compiler_options(C LVGL_TESTFILE_C_OPTIONS ${LVGL_C_COMPILE_OPTIONS}
                          -Wno-missing-prototypes)

  filter_compiler_options(
    CXX
    LVGL_CXX_COMPILE_OPTIONS
    ${COMPILE_OPTIONS}
    -Wno-shadow
    -Wno-unused-parameter
    -Wno-c++11-extensions
    -Wno-missing-prototypes
    -Wno-deprecated-copy-with-user-provided-dtor
    -Wno-float-conversion
    -Wno-pedantic)
else()
  filter_compiler_options(C LVGL_TESTFILE_C_OPTIONS ${LVGL_C_COMPILE_OPTIONS})
  filter_compiler_options(CXX LVGL_CXX_COMPILE_OPTIONS ${COMPILE_OPTIONS})
endif()

# Sanitizer link libs (only for test executables, not lvgl itself)
filter_compiler_options(C SANITIZER_LINK_OPTIONS ${SANITIZER_OPTIONS})

set(TEST_INCLUDE_DIRS
    $<BUILD_INTERFACE:${LVGL_TEST_DIR}/src>
    $<BUILD_INTERFACE:${LVGL_TEST_DIR}/unity>
    $<BUILD_INTERFACE:${LVGL_TEST_DIR}> $<BUILD_INTERFACE:${LVGL_CONF_DIR}>)

# =============================================================================
# Test registration functions
#
# add_tests(TESTS <name> ...) SW-only: registers tests under SW_SYSHEAP and
# SW_DEFHEAP configs. Use for logic/widget/API tests where rendering output is
# not the focus.
#
# add_render_tests(TESTS <name> ...) All backends: registers tests under every
# rendering config (SW_SYSHEAP, SW_DEFHEAP, VG_LITE, RISCV_V, ...). Use for
# tests that verify pixel output or drawing primitives.
#
# Both functions inherit TEST_LINK_LIBS, TEST_INCLUDE_DIRS,
# SANITIZER_LINK_OPTIONS, and LVGL_TESTFILE_C_OPTIONS from the parent scope.
# =============================================================================

set(SW_CONFIGS SW_SYSHEAP SW_DEFHEAP)
set(ALL_CONFIGS SW_SYSHEAP SW_DEFHEAP VG_LITE RISCV_V)

function(add_tests)
  cmake_parse_arguments(ARG "" "LABEL" "TESTS;EXTRA_LIBS" ${ARGN})

  if(NOT BUILD_CONFIG IN_LIST SW_CONFIGS)
    return()
  endif()

  _register_tests(LABEL "${ARG_LABEL}" TESTS ${ARG_TESTS} EXTRA_LIBS
                  ${ARG_EXTRA_LIBS})
endfunction()

function(add_render_tests)
  cmake_parse_arguments(ARG "" "LABEL" "TESTS;EXTRA_LIBS" ${ARGN})

  if(NOT BUILD_CONFIG IN_LIST ALL_CONFIGS)
    return()
  endif()

  _register_tests(LABEL "${ARG_LABEL}" TESTS ${ARG_TESTS} EXTRA_LIBS
                  ${ARG_EXTRA_LIBS})
endfunction()

function(_register_tests)
  cmake_parse_arguments(ARG "" "LABEL" "TESTS;EXTRA_LIBS" ${ARGN})

  add_unity_tests(
    SOURCES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIR
    ${LVGL_TEST_DIR}
    TESTS
    ${ARG_TESTS}
    LINK_LIBRARIES
    ${TEST_LINK_LIBS}
    ${SANITIZER_LINK_OPTIONS}
    ${ARG_EXTRA_LIBS}
    INCLUDE_DIRS
    ${TEST_INCLUDE_DIRS}
    LABELS
    "${ARG_LABEL};${BUILD_CONFIG}")
endfunction()

# =============================================================================
# Determine which tests to build/register based on BUILD_CONFIG
# =============================================================================

if(BUILD_CONFIG STREQUAL "SW_SYSHEAP"
   OR BUILD_CONFIG STREQUAL "SW_DEFHEAP"
   OR BUILD_CONFIG STREQUAL "RISCV_V")
  set(REF_IMGS_PATH "ref_imgs/")
elseif(BUILD_CONFIG STREQUAL "VG_LITE")
  # Only render tests make sense for a GPU backend
  set(REF_IMGS_PATH "ref_imgs_vg_lite/")
endif()

add_subdirectory(src)

# =============================================================================
# Convenience target: cmake --build . --target run
# =============================================================================
add_custom_target(
  run
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 300
  WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
  USES_TERMINAL)
