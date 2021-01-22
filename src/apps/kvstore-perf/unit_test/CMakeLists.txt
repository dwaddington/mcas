cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


project(unit_tests_stats CXX)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)


add_definitions(-DCONFIG_DEBUG)

add_executable(unit_tests_stats test_stats.cpp)
target_link_libraries(unit_tests_stats ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl)

project(unit_tests_stopwatch CXX)

add_executable(unit_tests_stopwatch test_stopwatch.cpp)
target_link_libraries(unit_tests_stopwatch ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl)

