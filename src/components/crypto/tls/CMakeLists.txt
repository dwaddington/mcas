cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

project(component-tls CXX)

set(CMAKE_CXX_STANDARD 14)

include (../../../../mk/clang-dev-tools.cmake)

add_subdirectory(./unit_test)

file(GLOB SOURCES src/*.cpp)

add_definitions(-DCONFIG_DEBUG)
add_compile_options(${FLAG_DUMP_CLASS})
add_compile_options(-Wno-conversion) # allow truncating conversions

#add_compile_options("-O0")

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libbase64/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

link_directories(${CMAKE_BINARY_DIR}/src/lib/libbase64)
link_directories(/usr/lib/x86_64-linux-gnu)

include_directories(../../../server/mcas/src/)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_compile_options(${PROJECT_NAME} PUBLIC "-fPIC")

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

target_link_libraries(${PROJECT_NAME} common pthread numa dl rt z gnutls base64) # optional 'profiler'

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
#install(FILES "mcas-privkey.pem" DESTINATION certs)
#install(FILES "mcas-cert.pem" DESTINATION certs)
#install(FILES "client-privkey.pem" DESTINATION certs)
#install(FILES "client-cert.pem" DESTINATION certs)

