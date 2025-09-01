cmake_minimum_required(VERSION 3.12)
project (lvgl_examples_webscripten)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -s USE_SDL=2")

include_directories(${PROJECT_SOURCE_DIR})

include_directories(${GSTREAMER_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

add_subdirectory(lvgl)

file(GLOB MY_SOURCES "./*.c")
set(SOURCES ${MY_SOURCES})

add_executable(index ${SOURCES} ${INCLUDES})

if(NOT LVGL_CHOSEN_DEMO)
    set(LVGL_CHOSEN_DEMO lv_demo_widgets)
endif()

set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS -DCHOSEN_DEMO=${LVGL_CHOSEN_DEMO})

set(CMAKE_EXECUTABLE_SUFFIX ".html")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)

include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
)

link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_VIDEO_LIBRARY_DIRS}
    ${GSTREAMER_APP_LIBRARY_DIRS}
)

target_link_libraries(index
    lvgl
    lvgl_examples
    lvgl_demos
    lvgl_thorvg
    SDL2
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
)

set_target_properties(index PROPERTIES LINK_FLAGS "--shell-file ${PROJECT_SOURCE_DIR}/lvgl_shell.html -s SINGLE_FILE=1 -s INITIAL_MEMORY=33554432")
