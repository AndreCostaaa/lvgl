import os


directories = os.listdir(".")

for dir in directories:
    if os.path.isdir(dir):
        files = os.listdir(dir)
        c_files = [file for file in files if file.endswith(".c")]
        with open(f"{dir}/CMakeLists.txt", "w") as f:
            f.write(f"")
            f.write(
                f"""set(SOURCE_FILES {"\n".join(c_files)})
# Create the main static library for this directory
add_library(lvgl_drivers_display_{dir} STATIC ${{SOURCE_FILES}})

# Add the current directory as an include directory
target_include_directories(lvgl_drivers_display_{dir} PUBLIC ${{CMAKE_CURRENT_SOURCE_DIR}})"""
            )

with open("CMakeLists.txt", "w") as f:
    f.write("set(SUBDIRS display)\n\n")
    for dir in directories:
        f.write(f"if(CONFIG_LV_USE_{dir.upper()})\n")
        f.write(f"\tlist(APPEND SUBDIRS {dir})\n")
        f.write(f"endif()\n\n")
    f.write(f"if(CONFIG_LV_USE_OPENGLES)\n")
    f.write(f"\tlist(APPEND SUBDIRS glfw)\n")
    f.write(f"endif()\n\n")
    f.write(
        """
# Add each subdirectory and link its library
foreach(subdir ${SUBDIRS})
   add_subdirectory(${subdir})
   target_link_libraries(lvgl_drivers_display PRIVATE ${subdir})
endforeach()
"""
    )

# files = os.listdir(".")
#
# with open("CMakeLists.txt", "w") as f:
#    f.write("set(SOURCE_FILES)")
#    for file in files:
#        if file.endswith(".c"):
#            f.write(f"if(CONFIG_{file.replace(".c", "").upper()})\n")
#            f.write(f"\tlist(APPEND SOURCE_FILES {file})\n")
#            f.write(f"endif()\n\n")
#    f.write(
#        """
## Create the main static library for this directory
# add_library(lvgl STATIC ${LVGL_SOURCES})
#
## Add the current directory as an include directory
# target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})"""
#    )
