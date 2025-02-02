import os


directories = os.listdir(".")

files = os.listdir(".")

c_files = [file for file in files if file.endswith(".c")]

with open("CMakeLists.txt", "w") as f:
    f.write(f"set(SOURCE_FILES lv_draw_sw_blend.c)\n")
    for file in c_files:
        if file == "lv_draw_sw_blend.c" or file == "lv_draw_sw_blen_to_i1.c":
            continue
        f.write(
            f"if(CONFIG_LV_DRAW_SW_SUPPORT_{file.replace(".c", "").split("_")[-1].upper()})\n"
        )
        f.write(f"\tlist(APPEND SOURCE_FILES {file})\n")
        f.write(f"endif()\n\n")
    f.write(
        """
# Create the main static library for this directory
add_library(lvgl_draw STATIC ${SOURCE_FILES})

# Add the current directory as an include directory
target_include_directories(lvgl_draw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})"""
    )

# with open("CMakeLists.txt", "w") as f:
#     f.write(f"set(SOURCE_FILES {"\n\t\t".join(c_files)})\n")
#     f.write(
#         """
# # Create the main static library for this directory
# add_library(lvgl_draw STATIC ${SOURCE_FILES})
#
# # Add the current directory as an include directory
# target_include_directories(lvgl_draw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})"""
#     )


# for dir in directories:
#     if os.path.isdir(dir):
#         files = os.listdir(dir)
#         c_files = [file for file in files if file.endswith(".c")]
#         with open(f"{dir}/CMakeLists.txt", "w") as f:
#             f.write(f"")
#             f.write(
#                 f"""set(SOURCE_FILES {"\n".join(c_files)})
# # Create the main static library for this directory
# add_library(lvgl_draw_{dir} STATIC ${{SOURCE_FILES}})
#
# # Add the current directory as an include directory
# target_include_directories(lvgl_draw_{dir} PUBLIC ${{CMAKE_CURRENT_SOURCE_DIR}})"""
#             )
#
# with open("CMakeLists.txt", "a") as f:
#     f.write("set(SUBDIRS display)\n\n")
#     for dir in directories:
#         f.write(f"if(CONFIG_LV_USE_DRAW_{dir.upper()})\n")
#         f.write(f"\tlist(APPEND SUBDIRS {dir})\n")
#         f.write(f"endif()\n\n")
#     f.write(
#         """
# # Add each subdirectory and link its library
# foreach(subdir ${SUBDIRS})
#    add_subdirectory(${subdir})
#    target_link_libraries(lvgl_draw PRIVATE ${subdir})
# endforeach()
# """
#     )
