# - Try to find HighwayHash.
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  HIGHWAYHASH_INCLUDE_DIR    Set this variable to the root directory of
#                              cuckoofilter if the module has problems finding
#                              the proper path.
#
# Variables defined by this module:
#
#  HIGHWAYHASH_FOUND          System has highwayhash libraries and headers.
#  HIGHWAYHASH_LIBRARY        The highwayhash library
#  HIGHWAYHASH_INCLUDE_DIR    The location of highwayhash headers

# use NO_DEFAULT_PATH as the install target of highwayhash is broken
# Place the git repo next to this project and do a make
find_library(HIGHWAYHASH_LIBRARY
    NAMES libhighwayhash.a
    PATHS ../highwayhash/lib
    NO_DEFAULT_PATH
)

find_path(HIGHWAYHASH_INCLUDE_DIR
    NAMES highwayhash/highwayhash.h
    PATHS ../highwayhash
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HighwayHash DEFAULT_MSG
    HIGHWAYHASH_LIBRARY
    HIGHWAYHASH_INCLUDE_DIR
)

mark_as_advanced(
    HIGHWAYHASH_LIBRARY
    HIGHWAYHASH_INCLUDE_DIR
)
