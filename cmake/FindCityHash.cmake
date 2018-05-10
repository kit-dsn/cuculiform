# - Try to find CityHash.
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CITYHASH_INCLUDE_DIR    Set this variable to the root directory of
#                          cuckoofilter if the module has problems finding
#                          the proper path.
#
# Variables defined by this module:
#
#  CITYHASH_FOUND          System has cityhash libraries and headers.
#  CITYHASH_LIBRARY        The cityhash library
#  CITYHASH_INCLUDE_DIR    The location of cityhash headers

# use NO_DEFAULT_PATH as the install target of cityhash is broken
# Place the git repo next to this project and do a make
find_library(CITYHASH_LIBRARY
    NAMES libcityhash.a
    PATHS ../cityhash/lib
    NO_DEFAULT_PATH
)

find_path(CITYHASH_INCLUDE_DIR
    NAMES city.h
    PATHS ../cityhash/include
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CityHash DEFAULT_MSG
    CITYHASH_LIBRARY
    CITYHASH_INCLUDE_DIR
)

mark_as_advanced(
    CITYHASH_LIBRARY
    CITYHASH_INCLUDE_DIR
)
