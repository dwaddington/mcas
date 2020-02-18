cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include(${CONF_COMANCHE_HOME}/mk/common.cmake)

project(mcas-test CXX)

include_directories(../include)
link_directories(../)
add_definitions(-DCONFIG_DEBUG)

add_executable(mcas-test ./test_client.cpp)
target_link_libraries(mcas-test ${ASAN_LIB} common comanche-core pthread numa dl rt z)

