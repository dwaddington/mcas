cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include(../../../mk/common.cmake)

set(CMAKE_CXX_STANDARD 14)

add_compile_options(-g)

# use this to disable optimizations, e.g. for debugging or profiling
add_compile_options("$<$<CONFIG:Debug>:-O0>")

#set(PROFILER "profiler")
#add_compile_options("-DPROFILE")

project(mcas CXX)
include(../../../mk/clang-dev-tools.cmake)

include_directories(include)
#include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/kernel/modules/xpmem/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libthreadipc/include/)
include_directories(${CMAKE_SOURCE_DIR}/src/components/)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # rapidjson
link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # tbb (via nupm)
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # tbb (via nupm)

add_definitions(-DCONFIG_DEBUG)
add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")

configure_file(example-ado.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/example-ado.conf)
configure_file(graph-ado.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/graph-ado.conf)

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

add_executable(mcas src/main.cpp src/shard.cpp src/connection_handler.cpp src/ado_manager.cpp src/security.cpp)

target_link_libraries(mcas ${ASAN_LIB} threadipc common numa pthread dl nupm boost_program_options crypto z xpmem ado-proto ${PROFILER} )

#add_subdirectory(unit_test)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}:${CMAKE_INSTALL_PREFIX}/lib)

add_dependencies(${PROJECT_NAME} threadipc common nupm)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES "example-ado.conf" DESTINATION bin)
install(FILES "graph-ado.conf" DESTINATION bin)
