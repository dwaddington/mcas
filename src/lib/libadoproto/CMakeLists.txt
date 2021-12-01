cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(ado-proto CXX)

set(CMAKE_CXX_STANDARD 17)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wdeprecated-copy-dtor COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings $<$<BOOL:${COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR}>:-Wdeprecated-copy-dtor>)

# use this to disable optimizations, e.g. for debugging or profiling
add_compile_options("$<$<CONFIG:Debug>:-O0>")

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated/)

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # flatbuffers

add_definitions(-DCONFIG_DEBUG)

set(SOURCES src/ado_proto.cpp src/uipc.cpp src/uipc_channel.cpp src/uipc_shared_memory.cpp src/ado_proto_buffer.cpp src/channel_wrap.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
target_link_libraries(${PROJECT_NAME} common numa dl rt boost_system pthread curl flatbuffers gcov)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  PUBLIC_HEADER include/ado_proto.h
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME}
  LIBRARY
  DESTINATION lib
  PUBLIC_HEADER DESTINATION include)

