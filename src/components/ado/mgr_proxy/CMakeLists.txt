cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adomgrproxy CXX)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(unit_test)

add_definitions(-DCONFIG_DEBUG)

include_directories(../../../lib/common/include)
include_directories(../../../lib/libadoproto/include)
include_directories(../../../components/)
include_directories(../../)
include_directories(../../../lib/libthreadipc/include)

file(GLOB SOURCES src/*.c*)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
target_link_libraries(${PROJECT_NAME} common numa dl rt boost_system pthread threadipc)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME}
    LIBRARY
    DESTINATION lib)

