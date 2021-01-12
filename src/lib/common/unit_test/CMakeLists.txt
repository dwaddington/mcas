cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(libcommon-test CXX)

find_library(GTEST_LIB NAMES gtest PATHS /usr/lib,/usr/lib64/,/usr/local/lib,/usr/local/lib64/)

if(NOT GTEST_LIB)
  message(FATAL_ERROR "gtest library not found:" ${GTEST_LIB})
else()
  message("Found gtest library:" ${GTEST_LIB})
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)

#link_directories(${CMAKE_SOURCE_DIR}/src/lib/common)
link_directories(/usr/lib64)

add_definitions(-DCONFIG_DEBUG)

add_executable(libcommon-test1 test1.cpp)

target_compile_options(libcommon-test1 PUBLIC $<$<CONFIG:Debug>:-O0> -g -pedantic -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2) # -Wconversion
target_link_libraries(libcommon-test1 ${GTEST_LIB} common boost_system pthread dl numa gcov )  # add profiler for google profiler
