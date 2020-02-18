cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (ado-prox-test CXX)

add_definitions(-DCONFIG_DEBUG)
add_compile_options(-Wnoerror)

add_executable(ado-prox-test test.cpp)


target_link_libraries(ado-prox-test boost_program_options
	profiler boost_system boost_date_time boost_filesystem gtest pthread
	common comanche-core dl numa)

