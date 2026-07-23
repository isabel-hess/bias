# - Try to find the Lucid Vision Labs Arena SDK (C API)
#
# Once done this will define
#
#  Arena_FOUND         - System has the Arena SDK
#  Arena_INCLUDE_DIRS  - The Arena C API include directories
#  Arena_LIBRARIES     - The libraries needed to use the Arena C API
#
# This module links against the Arena *C* API (ArenaC) so that the BIAS
# backend uses a flat C ABI - mirroring the Spinnaker C API backend.
# ------------------------------------------------------------------------------

if (WIN32)
    message(STATUS "FindArena system is WIN32")
    # The installer sets the LUCID_DEV_ROOT environment variable to the SDK root
    # e.g. "C:/Program Files/Lucid Vision Labs/Arena SDK/"
    set(typical_arena_dir "C:/Program Files/Lucid Vision Labs/Arena SDK")
    set(typical_arena_inc_dir "${typical_arena_dir}/include/ArenaC")
    set(typical_arena_lib_dir "${typical_arena_dir}/lib64/ArenaC")
    set(env_arena_inc_dir "$ENV{LUCID_DEV_ROOT}/include/ArenaC")
    set(env_arena_lib_dir "$ENV{LUCID_DEV_ROOT}/lib64/ArenaC")
else()
    message(STATUS "FindArena system is not WIN32")
    set(typical_arena_dir "/usr")
    set(typical_arena_inc_dir "${typical_arena_dir}/include/arena/ArenaC")
    set(typical_arena_lib_dir "${typical_arena_dir}/lib")
    set(env_arena_inc_dir "$ENV{LUCID_DEV_ROOT}/include/ArenaC")
    set(env_arena_lib_dir "$ENV{LUCID_DEV_ROOT}/lib64/ArenaC")
endif()

message(STATUS "finding Arena include dir")
find_path(
    Arena_INCLUDE_DIR
    "ArenaCApi.h"
    HINTS ${env_arena_inc_dir} ${typical_arena_inc_dir}
    )
message(STATUS "Arena_INCLUDE_DIR: " ${Arena_INCLUDE_DIR})

if(WIN32)
    message(STATUS "finding Arena library")
    find_library(
        Arena_LIBRARY
        NAMES "ArenaC_v140.lib" "ArenaC_v140"
        HINTS ${env_arena_lib_dir} ${typical_arena_lib_dir}
        )
else()
    message(STATUS "finding Arena library")
    find_library(
        Arena_LIBRARY
        NAMES "arenac"
        HINTS ${env_arena_lib_dir} ${typical_arena_lib_dir}
        )
endif()

set(Arena_LIBRARIES ${Arena_LIBRARY} )
set(Arena_INCLUDE_DIRS ${Arena_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set Arena_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(
    Arena  DEFAULT_MSG
    Arena_LIBRARY
    Arena_INCLUDE_DIR
    )

mark_as_advanced(Arena_INCLUDE_DIR Arena_LIBRARY )
