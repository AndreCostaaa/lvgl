find_program(
  RUBY_EXECUTABLE
  NAMES ruby
  DOC "Ruby interpreter")

if(NOT RUBY_EXECUTABLE)
  message(FATAL_ERROR "Ruby interpreter not found")
endif()

message(STATUS "Found Ruby: ${RUBY_EXECUTABLE}")

set(UNITY_VERSION "v2.6.1")
set(UNITY_AUTO_DIR "${CMAKE_BINARY_DIR}/_deps/unity-auto")
set(UNITY_AUTO_URL
    "https://raw.githubusercontent.com/ThrowTheSwitch/Unity/${UNITY_VERSION}/auto"
)

# Fetch Unity
FetchContent_Declare(
  unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG ${UNITY_VERSION})

FetchContent_MakeAvailable(unity)

# Unity doesn't have proper CMakeLists, create library if needed
if(NOT TARGET unity)
  add_library(unity STATIC ${unity_SOURCE_DIR}/src/unity.c)

  target_include_directories(unity
                             PUBLIC $<BUILD_INTERFACE:${unity_SOURCE_DIR}/src>)

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(unity PRIVATE -Wno-unused-parameter
                                         -Wno-missing-prototypes)
  endif()
endif()

# List of files needed from Unity's auto folder
set(UNITY_AUTO_FILES generate_test_runner.rb run_test.erb type_sanitizer.rb
                     yaml_helper.rb)

# Download all required Unity auto scripts
file(MAKE_DIRECTORY "${UNITY_AUTO_DIR}")

foreach(auto_file ${UNITY_AUTO_FILES})
  set(local_file "${UNITY_AUTO_DIR}/${auto_file}")

  if(NOT EXISTS "${local_file}")
    message(STATUS "Downloading Unity auto/${auto_file}...")
    file(DOWNLOAD "${UNITY_AUTO_URL}/${auto_file}" "${local_file}"
         STATUS download_status)
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
      message(FATAL_ERROR "Failed to download ${auto_file}")
      break()
    endif()
  endif()
endforeach()

# Make Unity paths available globally
set(UNITY_GENERATE_SCRIPT
    "${UNITY_AUTO_DIR}/generate_test_runner.rb"
    CACHE INTERNAL "")
set(UNITY_RUBY_EXECUTABLE
    ${RUBY_EXECUTABLE}
    CACHE INTERNAL "")

message(STATUS "Unity test framework ready")
