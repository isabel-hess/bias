# - Try to find Spinnaker 
# 
# Once done this will define
#
#  Spinnaker_FOUND         - System has LibXml2
#  Spinnaker_INCLUDE_DIRS  - The Spinnaker include directories
#  Spinnaker_LIBRARIES     - The libraries needed to use Spinnaker 
#
# ------------------------------------------------------------------------------

if (WIN32)
    message(STATUS "FindSpinnaker system is WIN232")
    # Spinnaker's install root has moved across versions:
    #   4.x    -> "C:/Program Files/Teledyne/Spinnaker"     (Teledyne rebrand)
    #   2.x-3.x-> "C:/Program Files/FLIR Systems/Spinnaker"
    #   1.x    -> "C:/Program Files/Point Grey Research/Spinnaker"
    # The installer also sets SPINNAKER_INSTALL_PATH, so prefer that when
    # present and fall back to the known locations, newest first.
    if(DEFINED ENV{SPINNAKER_INSTALL_PATH} AND EXISTS "$ENV{SPINNAKER_INSTALL_PATH}")
        file(TO_CMAKE_PATH "$ENV{SPINNAKER_INSTALL_PATH}" typical_spin_dir)
    elseif(EXISTS "C:/Program Files/Teledyne/Spinnaker")
        set(typical_spin_dir "C:/Program Files/Teledyne/Spinnaker")
    elseif(EXISTS "C:/Program Files/FLIR Systems/Spinnaker")
        set(typical_spin_dir "C:/Program Files/FLIR Systems/Spinnaker")
    else()
        set(typical_spin_dir "C:/Program Files/Point Grey Research/Spinnaker")
    endif()
    set(typical_spin_lib_dir "${typical_spin_dir}/lib64/vs2015")
    set(typical_spin_inc_dir "${typical_spin_dir}/include/spinc")
else()
    message(STATUS "FindSpinnaker system is not WIN232")
    set(typical_spin_dir "/usr")
    set(typical_spin_lib_dir "${typical_spin_dir}/lib")
    set(typical_spin_inc_dir "${typical_spin_dir}/include/spinnaker/spinc")
endif()

message(STATUS "${typical_spin_inc_dir}")

message(STATUS "finding include dir")
find_path(
    Spinnaker_INCLUDE_DIR 
    "SpinnakerC.h"
    HINTS ${typical_spin_inc_dir}
    )
message(STATUS "Spinnaker_INCLUDE_DIR: " ${Spinnaker_INCLUDE_DIR})

if(WIN32)
    message(STATUS "finding library")
    find_library(
        Spinnaker_LIBRARY 
        NAMES "SpinnakerC_v140.lib"
        HINTS ${typical_spin_lib_dir} 
        )
else() 
    message(STATUS "finding library")
    find_library(
        Spinnaker_LIBRARY 
        NAMES "libSpinnaker_C.so.1"
        HINTS ${typical_spin_lib_dir} 
        )
endif()


set(Spinnaker_LIBRARIES ${Spinnaker_LIBRARY} )
set(Spinnaker_INCLUDE_DIRS ${Spinnaker_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set Spinnaker_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(
    Spinnaker  DEFAULT_MSG
    Spinnaker_LIBRARY 
    Spinnaker_INCLUDE_DIR
    )

mark_as_advanced(Spinnaker_INCLUDE_DIR Spinnaker_LIBRARY )

