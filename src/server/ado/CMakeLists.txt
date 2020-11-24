cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (ado)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-DCONFIG_DEBUG)


#set(PROFILER "profiler")
#add_compile_options("-DPROFILE")
#target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")

add_definitions(-DCONFIG_DEBUG)
add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/components/ado/ado_proxy/src)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/generated)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # docker.h
include_directories(${CMAKE_SOURCE_DIR}/src/kernel/modules/xpmem/include)

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
link_directories(${CMAKE_INSTALL_PREFIX}/lib) 
link_directories(${CMAKE_SOURCE_DIR}/src/kernel/modules/xpmem/lib/.libs)

add_executable(ado src/ado.cpp)

target_link_libraries(ado pthread boost_program_options nupm xpmem ado-proto tbb ${PROFILER})

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

add_dependencies(${PROJECT_NAME} nupm)

install(TARGETS ado RUNTIME DESTINATION bin)
