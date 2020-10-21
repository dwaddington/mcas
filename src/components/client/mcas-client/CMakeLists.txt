cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include(../../../../mk/common.cmake)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

project(component-mcasclient CXX)

set(CMAKE_CXX_STANDARD 14)

include (../../../../mk/clang-dev-tools.cmake)

add_subdirectory(./unit_test)

# use this to disable optimizations, e.g. for debugging or profiling
add_compile_options("$<$<CONFIG:Debug>:-O0>")

file(GLOB SOURCES src/*.cpp)

add_definitions(-DCONFIG_DEBUG)
add_compile_options(${FLAG_DUMP_CLASS})

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/cityhash/cityhash/src)
include_directories(../)
include_directories(../../)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # city.h
link_directories(/usr/lib/x86_64-linux-gnu)

# Inclusion of Buffer_manager, at least
include_directories(../../../server/mcas/src/)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_compile_options(${PROJECT_NAME} PUBLIC "-fPIC")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

if( ${ARCHITECTURE} STREQUAL "x86_64" )
  target_link_libraries(${PROJECT_NAME} common pthread numa gnutls dl rt z) # optional 'profiler'
else()
  target_link_libraries(${PROJECT_NAME} common pthread numa gnutls dl rt z)
endif()

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
