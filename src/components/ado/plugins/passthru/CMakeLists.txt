cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-passthru CXX)

set(CMAKE_CXX_STANDARD 11)

enable_language(CXX C ASM)
set(GCC_COVERAGE_COMPILE_FLAGS "-g -O2 -fPIC")

file(GLOB SOURCES src/*.cpp)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings -Wformat=2)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(../../../../lib/common/include/)
include_directories(../../../)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/src)

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt)

set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


