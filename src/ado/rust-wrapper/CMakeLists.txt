cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-rustexample CXX)

set(CMAKE_CXX_STANDARD 14)

find_program(CARGO cargo)

file(GLOB SOURCES src/*.cpp)

add_definitions(-DCONFIG_DEBUG)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/libexample_rust_ado.a
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/example-rust-ado/src/lib.rs
  COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/example-rust-ado && ${CARGO} build --target-dir ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)


add_library(${PROJECT_NAME} SHARED ${SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/debug/libexample_rust_ado.a)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/src)

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt ${CMAKE_CURRENT_BINARY_DIR}/debug/libexample_rust_ado.a)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


