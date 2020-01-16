cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoproxy CXX)

set(CMAKE_CXX_STANDARD 14)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)


include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(../../../lib/common/include)
include_directories(../../../lib/libadoproto/include)
include_directories(../../)
# make sure install references are last
include_directories(${CMAKE_INSTALL_PREFIX}/include) # docker.h
link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # libdocker
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # libdocker


file(GLOB SOURCES src/*.c*)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
target_link_libraries(${PROJECT_NAME} common numa dl rt ado-proto
	boost_system pthread docker curl)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME}
    LIBRARY
    DESTINATION lib
    PUBLIC_HEADER DESTINATION include)

