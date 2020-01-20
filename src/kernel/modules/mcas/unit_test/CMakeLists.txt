cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (mcas-module-test CXX)

set (CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)
add_compile_options(-g -pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)
#add_compile_options(-Werror)

add_executable(${PROJECT_NAME} test.cpp)
add_executable(mcas-mod-client mcas_client.cpp)
add_executable(mcas-mod-server mcas_server.cpp)


#find_library(COMANCHE_DIST NAMES comanche-core common
# HINTS ${CMAKE_SOURCE_DIR}/comanche-dist/lib)
#find_library(COMANCHE_DIST NAMES common
# HINTS ${CMAKE_SOURCE_DIR}/comanche-dist/lib)


target_link_libraries(${PROJECT_NAME} boost_program_options
	profiler boost_system boost_date_time boost_filesystem gtest pthread
	common dl numa nupm tbb)

target_link_libraries(mcas-mod-client boost_program_options
	profiler boost_system boost_date_time boost_filesystem gtest pthread
	common dl numa nupm tbb)

target_link_libraries(mcas-mod-server boost_program_options
	profiler boost_system boost_date_time boost_filesystem gtest pthread
	common dl numa nupm tbb)

