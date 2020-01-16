cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(libccpm-test CXX)

set(CMAKE_CXX_STANDARD 14)
add_compile_options(-g -pedantic -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings)
add_compile_options("$<$<CONFIG:Debug>:-O0>")

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

add_executable(libccpm-test1 test1.cpp)
add_executable(libccpm-test2 test2.cpp)

target_compile_options(libccpm-test1 PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_compile_options(libccpm-test1 PUBLIC -g -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)

target_compile_options(libccpm-test2 PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_compile_options(libccpm-test2 PUBLIC -g -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)

target_link_libraries(libccpm-test1 ${ASAN_LIB} gtest pthread dl nupm)  # add profiler for google profiler
target_link_libraries(libccpm-test2 ${ASAN_LIB} gtest pthread dl nupm ccpm)  # add profiler for google profiler
