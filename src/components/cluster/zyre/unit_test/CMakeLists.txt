cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(zyre-tests CXX)
 
find_library(GTEST_LIB NAMES GTEST PATHS /usr/local/lib,/usr/lib)

add_compile_options("$<$<CONFIG:Debug>:-O0>")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

add_executable(zyre-test1 test1.cpp)
target_link_libraries(zyre-test1 ${ASAN_LIB} common numa gtest pthread dl gcov)

add_executable(zyre-test2 test2.cpp)
target_link_libraries(zyre-test2 ${ASAN_LIB} common numa pthread dl gcov boost_program_options)
