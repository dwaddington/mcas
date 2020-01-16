cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(libnupm-test CXX)

set(CMAKE_CXX_STANDARD 14)

set(GCC_COVERAGE_COMPILE_FLAGS "-std=c++14 -g -fPIC")

find_library(GTEST_LIB NAMES GTEST PATHS /usr/local/lib,/usr/lib)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(../src)

link_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/)
link_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/)
link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # ndctl
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # ndctl
link_directories(${GTEST_LIB})

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

add_executable(libnupm-test test1.cpp)
target_link_libraries(libnupm-test ${ASAN_LIB} common numa ndctl gtest pthread dl nupm pmem z)  # add profiler for google profiler

add_executable(libnupm-test2 test2.cpp)
target_compile_options(libnupm-test2 PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_compile_options(libnupm-test2 PUBLIC -g -pedantic -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)
target_link_libraries(libnupm-test2 ${ASAN_LIB} gtest pthread dl nupm)  # add profiler for google profiler
