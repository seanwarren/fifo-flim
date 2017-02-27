# Try to find BeckerHickl library
# Once done this will define
#  BeckerHickl_FOUND - if system found BeckerHickl library
#  BeckerHickl_INCLUDE_DIRS - The BeckerHickl include directories
#  BeckerHickl_LIBRARIES - The libraries needed to use BeckerHickl
#  BeckerHickl_DEFINITIONS - Compiler switches required for using BeckerHickl

set (BeckerHickl_ROOT_DIR "C:/Program Files (x86)/BH/SPCM/DLL")
# Uncomment the following line to print which directory CMake is looking in.
#MESSAGE(STATUS "BeckerHickl_ROOT_DIR: " ${BeckerHickl_ROOT_DIR})

find_path(BeckerHickl_INCLUDE_DIR
    NAMES TimeTagger4_interface.h
    PATHS ${BeckerHickl_ROOT_DIR}/include
    DOC "The BeckerHickl include directory"
)

find_library(BeckerHickl_LIBRARY 
    NAMES xtdc4_driver_64
    PATHS ${BeckerHickl_ROOT_DIR}/x64/lib
    DOC "The timetagger library"
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOGGING_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(BeckerHickl DEFAULT_MSG BeckerHickl_INCLUDE_DIR BeckerHickl_LIBRARY)

if (BeckerHickl_FOUND)
    set(BeckerHickl_LIBRARIES ${BeckerHickl_LIBRARY})
    set(BeckerHickl_INCLUDE_DIRS ${BeckerHickl_INCLUDE_DIR})
    set(BeckerHickl_DEFINITIONS "-DUSE_BECKERHICKL")
endif()

# Tell cmake GUIs to ignore the "local" variables.
mark_as_advanced(BeckerHickl_ROOT_DIR BeckerHickl_INCLUDE_DIR BeckerHickl_LIBRARY)