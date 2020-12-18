cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(personality-tabulator-ccpm CXX)

find_program(FLATC NAMES flatc PATHS "${CMAKE_INSTALL_PREFIX}/bin" NO_DEFAULT_PATH)

if (NOT FLATC)
  message(FATAL_ERROR ": Flatc compiler not found")
endif()

set(GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated/tabulator_generated.h)

set(PLUGIN_SOURCES ${GENERATED} src/tabulator_plugin.cpp)
set(CLIENT_SOURCES ${GENERATED} )  

  
set(TEST_SOURCES ./src/tabulator_test.cpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/common)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # EASTL

#link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # flatbuffers
# don't use install directory
link_directories(${CMAKE_BINARY_DIR}/src/lib/flatbuffers/)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

add_custom_command(
  OUTPUT ${GENERATED}  
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/tabulator.fbs
  COMMAND ${FLATC} --python --cpp -o ${CMAKE_CURRENT_BINARY_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/tabulator.fbs
  COMMAND touch ${GENERATED}
)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options("$<$<CONFIG:Debug>:-O0>")
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})
target_link_libraries(${PROJECT_NAME} common pthread numa dl rt flatbuffers pmem ccpm tbb tbbmalloc )
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_BINARY_DIR}/src/lib/flatbuffers/) #:${CMAKE_INSTALL_PREFIX}/lib)

configure_file(tabulator-ccpm.conf.in ${CMAKE_CURRENT_BINARY_DIR}/tabulator-ccpm.conf)
configure_file(src/tabulator_ccpm_test.sh.in ${CMAKE_CURRENT_BINARY_DIR}/tabulator_ccpm_test.sh)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
  DESTINATION conf
  FILES_MATCHING
  PATTERN "*.conf"
  PATTERN CMakeFiles EXCLUDE
  PATTERN generated EXCLUDE
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
  DESTINATION lib
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN CMakeFiles EXCLUDE
  PATTERN generated EXCLUDE
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)

install(FILES src/tabulator_ccpm_test.py
  DESTINATION lib)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tabulator_ccpm_test.sh
  PERMISSIONS OWNER_EXECUTE OWNER_READ WORLD_EXECUTE WORLD_READ
  DESTINATION bin)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/Proto
  DESTINATION lib64/python)



