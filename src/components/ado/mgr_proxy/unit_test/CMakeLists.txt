cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (ado-prox-test CXX)

add_definitions(-DCONFIG_DEBUG)

include_directories(${CMAKE_SOURCE_DIR}/src/components)

include_directories(${CMAKE_INSTALL_PREFIX}/include)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_executable(ado-prox-test test.cpp)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

target_link_libraries(ado-prox-test boost_program_options
	profiler boost_system boost_date_time boost_filesystem ${GTEST_LIB} pthread
	common dl numa)

