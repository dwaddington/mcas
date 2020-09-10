cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(component-adoplugin-testing CXX)

set(CMAKE_CXX_STANDARD 14) # 14 for auto deduction of return type

enable_language(CXX C ASM)

file(GLOB SOURCES src/*.cpp)

add_definitions(-DCONFIG_DEBUG)
add_compile_options(${FLAG_DUMP_CLASS})
add_compile_options(-O2 -fPIC)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/EASTL/test/packages/EABase)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/components/)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/src)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # EASTL

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # pmem tbb tbbmalloc
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # pmem tbb tbbmalloc

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt pmem tbb tbbmalloc)

set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)


