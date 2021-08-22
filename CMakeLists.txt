cmake_minimum_required(VERSION 3.16)
project(mapvxl C)

set(CMAKE_C_STANDARD 17)

add_compile_options(-Wall -Wextra -std=c17 -O2)

add_library(mapvxl STATIC libmapvxl.c)

target_include_directories(mapvxl PUBLIC .)