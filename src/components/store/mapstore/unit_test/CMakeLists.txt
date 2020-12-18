cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


project(mapstore-tests CXX)

include_directories(../../../../lib/common/include)
include_directories(../../../)

add_compile_options(-O2 -fPIC)
add_executable(mapstore-test1 test1.cpp)
target_link_libraries(mapstore-test1 ${ASAN_LIB} common numa gtest pthread dl)

