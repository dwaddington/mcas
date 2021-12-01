cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(cw CXX)

set(CMAKE_CXX_STANDARD 17)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wdeprecated-copy-dtor COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings $<$<BOOL:${COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR}>:-Wdeprecated-copy-dtor>)

include(../../../mk/clang-dev-tools.cmake)

# configure_file(include/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h)

# execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

include_directories(./include)
include_directories(../common/include)
include_directories(../../components)
include_directories(../GSL/include)
# Temp, for cw
#include_directories(../../components)
# End temp
include_directories(${CMAKE_INSTALL_PREFIX}/include) # daxctl

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_INCLUDE_CURRENT_DIR true)
set(CMAKE_INSTALL_RPATH "/usr/lib64")

add_definitions(-DCONFIG_DEBUG)

set(SOURCES
  src/cw_common.cpp
  src/cw_fabric_test.cpp
  src/registered_memory.cpp
  src/registration.cpp
  src/remote_memory_accessor.cpp
  src/remote_memory_client_connected.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(${PROJECT_NAME}-static STATIC ${SOURCES})

target_compile_options(${PROJECT_NAME} PUBLIC "-fPIC")
target_compile_options(${PROJECT_NAME}-static PUBLIC "-fPIC")

target_link_libraries(${PROJECT_NAME} common) # dl gcov numa)

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(TARGETS ${PROJECT_NAME}-static ARCHIVE DESTINATION lib)
install(DIRECTORY "src/" DESTINATION "include/cw" FILES_MATCHING PATTERN "./include/cw/*.h")
install(DIRECTORY "include/cw/" DESTINATION "include/cw" FILES_MATCHING PATTERN "*.h")
