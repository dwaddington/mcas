cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(common CXX)

add_compile_options(-g -pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)

add_subdirectory(unit_test)

include(../../../mk/clang-dev-tools.cmake)

set(CMAKE_CXX_STANDARD 14)

configure_file(include/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

include_directories(./include)

set(CMAKE_INCLUDE_CURRENT_DIR true)
set(CMAKE_INSTALL_RPATH "/usr/lib64")

if( ${ARCHITECTURE} STREQUAL "x86_64" )
  add_compile_options(-msse4.2)
endif()

add_definitions(-DCONFIG_DEBUG)

file(GLOB SOURCES src/*.c*)

add_library(common SHARED ${SOURCES})
target_compile_options(common PUBLIC "-fPIC")
target_link_libraries(common gcov numa)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY "include/common" DESTINATION include FILES_MATCHING PATTERN "*.h*")
install(DIRECTORY "include/component" DESTINATION include FILES_MATCHING PATTERN "*.h*")
