cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(mm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG  "-g")

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings)
add_definitions(-DDM_REGION_LOG_GRAIN_SIZE=${DM_REGION_LOG_GRAIN_SIZE})

#include(../../../mk/clang-dev-tools.cmake)

include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # daxctl
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

add_definitions(-DCONFIG_DEBUG)

file(GLOB SOURCES src/mm_wrapper.cpp)

add_library(mm SHARED ${SOURCES})

target_compile_options(${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>:-O0>)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Weffc++ -Wold-style-cast>)
target_link_libraries(${PROJECT_NAME} common pthread dl)


# plain executable, we use LD_PRELOAD
add_executable(mmwrapper-test-prog src/test_prog.cpp)
target_compile_options(mmwrapper-test-prog PUBLIC $<$<CONFIG:Debug>:-O0>)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Weffc++ -Wold-style-cast> -fpermissive)


install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
