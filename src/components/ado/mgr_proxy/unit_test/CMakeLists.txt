cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (ado-prox-test CXX)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)
add_compile_options(-g -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)
#add_compile_options(-Werror)

include_directories(../../../../lib/common/include)
include_directories(../../../../lib/libadoproto/include)
include_directories(../../../../components/)
include_directories(../../../)

add_executable(ado-prox-test test.cpp)


#find_library(COMANCHE_DIST NAMES comanche-core common 
#	HINTS ${CMAKE_SOURCE_DIR}/comanche-dist/lib)
#find_library(COMANCHE_DIST NAMES common 
#	HINTS ${CMAKE_SOURCE_DIR}/comanche-dist/lib)


target_link_libraries(ado-prox-test boost_program_options
	profiler boost_system boost_date_time boost_filesystem gtest pthread
	common dl numa)

