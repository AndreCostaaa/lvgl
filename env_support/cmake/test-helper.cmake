# Helper function to create tests with Unity runner generation

set(UNITY_CONFIG_YML ${CMAKE_CURRENT_LIST_DIR}/unity/config.yml)

function(add_unity_tests)
  set(options "")
  set(oneValueArgs TARGET_LIB SOURCES_DIR WORKING_DIR)
  set(multiValueArgs TESTS COMMON_SOURCE_FILES INCLUDE_DIRS LINK_LIBRARIES
                     LABELS)

  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  foreach(test_name ${ARG_TESTS})
    set(test_source "${ARG_SOURCES_DIR}/${test_name}.c")
    set(test_runner "${CMAKE_CURRENT_BINARY_DIR}/${test_name}_runner.c")

    set(COMMAND "${UNITY_RUBY_EXECUTABLE} ${UNITY_GENERATE_SCRIPT}
                ${test_source} ${test_runner}")

    add_custom_command(
      OUTPUT ${test_runner}
      COMMAND ${UNITY_RUBY_EXECUTABLE} ${UNITY_GENERATE_SCRIPT} ${test_source}
              ${test_runner} ${UNITY_CONFIG_YML}
      DEPENDS ${test_source} ${UNITY_GENERATE_SCRIPT}
      COMMENT "Generating Unity test runner for ${test_name}"
      VERBATIM)

    message(
      STATUS
        "Source files are ${test_source} ${test_runner} ${ARG_COMMON_SOURCE_FILES}"
    )
    add_executable(${test_name} ${test_source} ${test_runner}
                                ${ARG_COMMON_SOURCE_FILES})

    list(APPEND ANIMATRONIC_TEST_TARGETS ${test_name})
    set(ANIMATRONIC_TEST_TARGETS
        ${ANIMATRONIC_TEST_TARGETS}
        CACHE INTERNAL "")

    # Link libraries
    target_link_libraries(
      ${test_name} PRIVATE unity test_common ${ARG_TARGET_LIB}
                           ${ARG_LINK_LIBRARIES})

    # Include directories
    if(ARG_INCLUDE_DIRS)
      target_include_directories(${test_name} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    # Register test with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Add coverage flags
    target_compile_options(${test_name} PRIVATE --coverage)
    target_link_options(${test_name} PRIVATE --coverage)

    # Set properties
    set(test_properties "")

    if(ARG_LABELS)
      list(APPEND test_properties LABELS "${ARG_LABELS}")
    endif()

    if(ARG_WORKING_DIR)
      list(APPEND test_properties WORKING_DIRECTORY "${ARG_WORKING_DIR}")
    endif()

    if(test_properties)
      set_tests_properties(${test_name} PROPERTIES ${test_properties})
    endif()

    add_dependencies(${test_name} unity)
  endforeach()
endfunction()
