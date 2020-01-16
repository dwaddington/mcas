cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(personality-example-fb CXX)

set(CMAKE_CXX_STANDARD 14)

find_program(FLATC NAMES flatc PATHS "${CMAKE_INSTALL_PREFIX}/bin")

if (NOT FLATC)
  message(FATAL_ERROR ": Flatc compiler not found")
endif()

set(CLIENT_SOURCES ./generated/example_fb_proto.h ./src/example_fb_client.cpp)
set(PLUGIN_SOURCES ./generated/example_fb_proto.h ./src/example_fb_plugin.cpp)
set(TEST_SOURCES ./src/example_fb_test.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/common)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # flatbuffers

#add_compile_options("$<$<CONFIG:Debug>:-O0>")

add_custom_command(
  OUTPUT ./generated/example_fb_proto.h
  DEPENDS src/example_fb_proto.fbs
  COMMAND ${FLATC} --cpp -o ${CMAKE_CURRENT_SOURCE_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/example_fb_proto.fbs )

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})
add_library(${PROJECT_NAME}-client SHARED ${CLIENT_SOURCES})
add_executable(${PROJECT_NAME}-test ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt flatbuffers )
target_link_libraries(${PROJECT_NAME}-client common pthread numa dl rt flatbuffers)
target_link_libraries(${PROJECT_NAME}-test common pthread numa dl rt flatbuffers ${PROJECT_NAME}-client)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set_target_properties(${PROJECT_NAME}-test PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install (TARGETS ${PROJECT_NAME}-client LIBRARY DESTINATION lib)
install (TARGETS ${PROJECT_NAME}-test RUNTIME DESTINATION bin)

configure_file(example-fb.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/example-fb.conf)

install(DIRECTORY ./
  DESTINATION bin
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)
