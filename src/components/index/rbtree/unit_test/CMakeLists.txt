cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(ramrbtree-tests CXX)

set(CMAKE_CXX_STANDARD 14)

include_directories(../../../../lib/common/include/)
include_directories(../../../)

add_executable(ramrbtree-test1 test1.cpp)
target_link_libraries(ramrbtree-test1 ${ASAN_LIB} common numa gtest pthread dl)

