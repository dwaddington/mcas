cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(test-libnop)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

include_directories(${PROJECT_SOURCE_DIR}/../../src/lib/libnop/include)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)


add_executable(test-libnop src/main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
