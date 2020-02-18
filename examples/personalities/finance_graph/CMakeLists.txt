cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

project(personality-finex CXX)

set(CMAKE_CXX_STANDARD 14)
set(CLIENT_SOURCES ./src/finex_client.cpp ./generated/finex_proto_generated.h)
set(SOURCES generated/finex_proto_generated.h src/finex_plugin.cpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/test/packages/EABase/include/Common/)

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # flatbuffers

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)
add_compile_options("$<$<CONFIG:Debug>:-O0>")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/finex_proto_generated.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/finex_proto.fbs 
  COMMAND ${FLATC} --cpp -o ${CMAKE_CURRENT_BINARY_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/finex_proto.fbs )

add_executable(${PROJECT_NAME}-client ${CLIENT_SOURCES})
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME}-client common pthread numa dl rt flatbuffers boost_program_options)
target_link_libraries(${PROJECT_NAME} common pthread numa dl rt flatbuffers)

set_target_properties(${PROJECT_NAME}-client PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

configure_file(finex-example.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/finex-example.conf)

install(CODE "execute_process(COMMAND tar -zxvf ${CMAKE_CURRENT_SOURCE_DIR}/data/example-data-500.tar.gz --directory ${CMAKE_CURRENT_BINARY_DIR} )")
install(TARGETS ${PROJECT_NAME}-client RUNTIME DESTINATION bin)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DESTINATION data/
  FILES_MATCHING PATTERN "*.csv"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)

install(DIRECTORY ./
  DESTINATION conf
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)


