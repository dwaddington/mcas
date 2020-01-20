cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-testing CXX)

set(CMAKE_CXX_STANDARD 14) # 14 for auto deduction of return type

enable_language(CXX C ASM)
set(GCC_COVERAGE_COMPILE_FLAGS "-g -O2 -fPIC")

file(GLOB SOURCES src/*.cpp)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings -Wformat=2)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/test/packages/EABase)
include_directories(../../../../lib/common/include/)
include_directories(../../../)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/src)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # EASTL

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # pmem tbb tbbmalloc
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # pmem tbb tbbmalloc

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt pmem tbb tbbmalloc)

set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


