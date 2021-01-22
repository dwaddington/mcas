cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(zyre-tests CXX)
 
find_library(GTEST_LIB NAMES GTEST PATHS /usr/local/lib,/usr/lib)

add_compile_options("$<$<CONFIG:Debug>:-O0>")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

include_directories(${CMAKE_INSTALL_PREFIX}/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_executable(zyre-test1 test1.cpp)
target_link_libraries(zyre-test1 ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl gcov)

add_executable(zyre-test2 test2.cpp)
target_link_libraries(zyre-test2 ${ASAN_LIB} common numa pthread dl gcov boost_program_options)
