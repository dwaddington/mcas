cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(libthreadipc-test CXX)

set(CMAKE_CXX_STANDARD 14)

find_library(GTEST_LIB NAMES GTEST PATHS /usr/local/lib,/usr/lib)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libthreadipc/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # daxctl, tbb
include_directories(../src)

link_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/)
link_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/)
link_directories(${CMAKE_SOURCE_DIR}/src/lib/libthreadipc/)
link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # ndctl
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # ndctl
link_directories(${GTEST_LIB})

add_executable(libthreadipc-test4 test4.cpp)
target_compile_options(libthreadipc-test4 PUBLIC "$<$<CONFIG:Debug>:-O0>")

#target_compile_options(libthreadipc-test4 PUBLIC -g -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)
target_link_libraries(libthreadipc-test4 ${ASAN_LIB} gtest pthread dl nupm threadipc boost_system)  # add profiler for google profiler
