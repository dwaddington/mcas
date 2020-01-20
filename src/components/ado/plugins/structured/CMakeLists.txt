cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-structured CXX)

set(CMAKE_CXX_STANDARD 14)

find_program(FLATC NAMES flatc PATHS "${CMAKE_INSTALL_PREFIX}/bin")

if (NOT FLATC)
  message(FATAL_ERROR ": Flatc compiler not found")
endif()

set(SOURCES generated/structured_proto_generated.h src/structured.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnop/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # EASTL

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings -Wformat=2)

add_compile_options("$<$<CONFIG:Debug>:-O0>")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/generated/structured_proto_generated.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/structured_proto.fbs
  COMMAND ${FLATC} --cpp -o ${CMAKE_CURRENT_SOURCE_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/structured_proto.fbs )

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt)

set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


