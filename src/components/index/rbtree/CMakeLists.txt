cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-index-rbtree CXX)

add_definitions(-DCONFIG_DEBUG)

include(../../../../mk/clang-dev-tools.cmake)

add_subdirectory(./unit_test)

include_directories(../../../lib/common/include)
include_directories(../../../lib/GSL/include)
include_directories(../../)

enable_language(CXX C ASM)
file(GLOB SOURCES src/*.c*)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_compile_options(${PROJECT_NAME} PUBLIC "-fPIC")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
target_link_libraries(${PROJECT_NAME} common numa dl rt boost_system pthread)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME}
    LIBRARY
    DESTINATION lib)

