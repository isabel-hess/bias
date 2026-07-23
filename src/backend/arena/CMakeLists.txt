cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(bias_backend_arena)

set(
    bias_backend_arena_SOURCE
    utils_arena.cpp
    node_map_utils_arena.cpp
    system_arena.cpp
    guid_device_arena.cpp
    camera_info_arena.cpp
    camera_device_arena.cpp
    )

add_library(bias_backend_arena ${bias_backend_arena_SOURCE})

target_link_libraries(
    bias_backend_arena
    ${bias_ext_link_LIBS}
    bias_backend_base
    bias_camera_facade
    )
