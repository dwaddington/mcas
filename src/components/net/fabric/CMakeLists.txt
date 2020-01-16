cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-fabric CXX)

set (CMAKE_CXX_STANDARD 14)

add_subdirectory(./unit_test)

add_compile_options(-g -pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)

include_directories(${CMAKE_INSTALL_PREFIX}/include) # rdma
include_directories(../../../lib/common/include/)
include_directories(../../../components)
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # libfabric

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

file(GLOB SOURCES src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_link_libraries(${PROJECT_NAME} common fabric pthread)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install (TARGETS ${PROJECT_NAME}
    LIBRARY
    DESTINATION lib)
