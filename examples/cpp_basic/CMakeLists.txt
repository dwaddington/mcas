cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(example-cpp-basic CXX)

add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

add_executable(example-cpp-basic cpp_basic.cpp)
target_link_libraries(example-cpp-basic common numa pthread boost_program_options dl)

set_target_properties(example-cpp-basic PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS example-cpp-basic RUNTIME DESTINATION bin)
