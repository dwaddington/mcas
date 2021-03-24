cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

project (kv-test)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

link_directories(${CMAKE_BINARY_DIR}/src/lib/common)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)
#add_compile_options(-g -pedantic -Wall -Werror -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings)

file(GLOB SOURCES src/*.c*)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} common numa pthread dl boost_program_options boost_system boost_date_time boost_filesystem ${GTEST_LIB})

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
