cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(libcommon-test CXX)

set(CMAKE_CXX_STANDARD 14)

find_library(GTEST_LIB NAMES GTEST PATHS /usr/local/lib,/usr/lib)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)

link_directories(${CMAKE_SOURCE_DIR}/src/lib/common)
link_directories(${GTEST_LIB})

add_definitions(-DCONFIG_DEBUG)

add_executable(libcommon-test1 test1.cpp)
target_compile_options(libcommon-test1 PUBLIC $<$<CONFIG:Debug>:-O0> -g -pedantic -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2) # -Wconversion
target_link_libraries(libcommon-test1 common gtest boost_system pthread dl numa gcov)  # add proflier for google profiler
