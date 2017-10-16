cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED  On)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")

project(cuculiform)

include_directories(src)

# TODO: WHY IS THIS NEEDED TO GET FIND_PACKAGE TO WORK?!?!?!?!?
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(HighwayHash REQUIRED)

add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE tests/catch)

add_executable(cuculiform_tests tests/tests.cc)
target_include_directories(cuculiform_tests PRIVATE ${HIGHWAYHASH_INCLUDE_DIR})
target_link_libraries(cuculiform_tests ${HIGHWAYHASH_LIBRARY})
target_link_libraries(cuculiform_tests Catch)

enable_testing()
add_test(NAME "CuculiformTests" COMMAND cuculiform_tests)
