cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-demo CXX)

enable_language(CXX C ASM)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wdeprecated-copy-dtor COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings $<$<BOOL:${COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR}>:-Wdeprecated-copy-dtor>)

file(GLOB SOURCES src/*.cpp)

add_definitions(-DCONFIG_DEBUG)
add_compile_options(${FLAG_DUMP_CLASS})
add_compile_options(-O2 -fPIC)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/src)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt)

set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


