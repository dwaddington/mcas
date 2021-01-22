cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


project(mcas-client-test CXX)

enable_language(CXX C ASM)

add_definitions(-DCONFIG_DEBUG)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_executable(mcas-client-test1 test1.cpp)
target_link_libraries(mcas-client-test1 ${ASAN_LIB} common numa ${GTEST_LIB} pthread dl boost_system boost_program_options)

set_target_properties(mcas-client-test1 PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS mcas-client-test1 RUNTIME DESTINATION bin)
