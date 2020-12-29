cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(personality-cpp-symtab CXX)

include(${CMAKE_SOURCE_DIR}/mk/avoid_boost_type_to_string_bug.cmake)

find_program(FLATC NAMES flatc PATHS "${CMAKE_INSTALL_PREFIX}/bin" NO_DEFAULT_PATH)

if (NOT FLATC)
  message(FATAL_ERROR ": Flatc compiler not found")
endif()

#set(CLIENT_SOURCES ./src/cpp_symtab_client.cpp ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp_symtab_proto.h)
set(PLUGIN_SOURCES ./src/cpp_symtab_plugin.cpp ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp_symtab_proto.h)
set(TEST_SOURCES ./src/cpp_symtab_test.cpp ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp_symtab_proto.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/common)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnop/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # EASTL

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # flatbuffers

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp_symtab_proto.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_symtab_proto.fbs
  COMMAND ${FLATC} --cpp -o ${CMAKE_CURRENT_BINARY_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_symtab_proto.fbs
  COMMAND touch ${CMAKE_CURRENT_BINARY_DIR}/generated/cpp_symtab_proto.h
  )

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})
#add_library(${PROJECT_NAME}-client SHARED ${CLIENT_SOURCES})
add_executable(${PROJECT_NAME}-test ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt)
#target_link_libraries(${PROJECT_NAME}-client common pthread numa dl rt flatbuffers)
target_link_libraries(${PROJECT_NAME}-test common pthread numa dl rt flatbuffers pmem boost_program_options )

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set_target_properties(${PROJECT_NAME}-test PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
#install(TARGETS ${PROJECT_NAME}-client LIBRARY DESTINATION lib)
install(TARGETS ${PROJECT_NAME}-test RUNTIME DESTINATION bin)

configure_file(cpp-symtab.conf.in ${CMAKE_CURRENT_BINARY_DIR}/cpp-symtab.conf)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
  DESTINATION conf
  FILES_MATCHING
  PATTERN "*.conf"
  PATTERN CMakeFiles EXCLUDE
  PATTERN generated EXCLUDE
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/google-10000-english.txt
  DESTINATION data/words/)

