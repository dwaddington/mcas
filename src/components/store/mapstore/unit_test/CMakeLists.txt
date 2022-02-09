cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


project(mapstore-tests CXX)

include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

set(GTEST_LIB "gtest$<$<CONFIG:Debug>:d>")

# testa moved to store/tests

