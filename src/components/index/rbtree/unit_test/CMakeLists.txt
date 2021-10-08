cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(ramrbtree-tests CXX)


include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

set(GTEST_LIB "gtest$<$<CONFIG:Debug>:d>")

add_executable(ramrbtree-test1 test1.cpp)
target_link_libraries(ramrbtree-test1 ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl)

