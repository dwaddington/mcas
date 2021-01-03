cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-fabric CXX)

add_compile_options("$<$<CONFIG:Debug>:-O0>")

add_subdirectory(./unit_test)

add_definitions(-DCONFIG_DEBUG) # P{LOG,DEG,INF,WRN,ERR} control

include_directories(${CMAKE_INSTALL_PREFIX}/include) # rdma
include_directories(../../../lib/common/include/)
include_directories(../../../components)
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # libfabric

file(GLOB SOURCES src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_compile_options(${PROJECT_NAME} PUBLIC -fPIC)
target_link_libraries(${PROJECT_NAME} common fabric pthread)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install (TARGETS ${PROJECT_NAME}
    LIBRARY
    DESTINATION lib)
