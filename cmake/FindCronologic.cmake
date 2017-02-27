# Try to find CRONOLOGIC library
# Once done this will define
#  CRONOLOGIC_FOUND - if system found CRONOLOGIC library
#  CRONOLOGIC_INCLUDE_DIRS - The CRONOLOGIC include directories
#  CRONOLOGIC_LIBRARIES - The libraries needed to use CRONOLOGIC
#  CRONOLOGIC_DEFINITIONS - Compiler switches required for using CRONOLOGIC

set (Cronologic_ROOT_DIR "C:/Program Files/cronologic/TimeTagger4/driver")
# Uncomment the following line to print which directory CMake is looking in.
#MESSAGE(STATUS "CRONOLOGIC_ROOT_DIR: " ${CRONOLOGIC_ROOT_DIR})

find_path(Cronologic_INCLUDE_DIR
    NAMES TimeTagger4_interface.h
    PATHS ${Cronologic_ROOT_DIR}/include
    DOC "The CRONOLOGIC include directory"
)

find_library(Cronologic_LIBRARY 
    NAMES xtdc4_driver_64
    PATHS ${Cronologic_ROOT_DIR}/x64/lib
    DOC "The timetagger library"
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOGGING_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Cronologic DEFAULT_MSG Cronologic_INCLUDE_DIR Cronologic_LIBRARY)

if (Cronologic_FOUND)
    set(Cronologic_LIBRARIES ${Cronologic_LIBRARY})
    set(Cronologic_INCLUDE_DIRS ${Cronologic_INCLUDE_DIR})
    set(Cronologic_DEFINITIONS "-DUSE_CRONOLOGIC")
endif()

# Tell cmake GUIs to ignore the "local" variables.
mark_as_advanced(Cronologic_ROOT_DIR Cronologic_INCLUDE_DIR Cronologic_LIBRARY)