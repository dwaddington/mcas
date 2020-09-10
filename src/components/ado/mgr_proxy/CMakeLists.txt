cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adomgrproxy CXX)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(unit_test)

add_definitions(-DCONFIG_DEBUG)

include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libthreadipc/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # TBB

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

