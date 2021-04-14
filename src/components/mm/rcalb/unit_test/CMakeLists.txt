cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-mm-rcalb CXX)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_executable(mm-rcalb-test1 test1.cpp)
target_link_libraries(mm-rcalb-test1 ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl)

