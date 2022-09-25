cmake_minimum_required(VERSION 3.16)

project(mapvxl C)

add_library(mapvxl STATIC)

target_sources(mapvxl
    PRIVATE
        "libmapvxl.h"
        "libmapvxl.c"
)

target_compile_features(mapvxl
    PUBLIC
        c_std_11
)

target_compile_options(mapvxl
    PRIVATE
        # Add Warnings and Optimization Levels (do not add for MSVC)
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra $<IF:$<CONFIG:Debug>,-Og,-O2>>
)

target_include_directories(mapvxl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
