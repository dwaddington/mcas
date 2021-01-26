cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(example-cpp-bench CXX)

#add_compile_options(-O0 -g)
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} -DCONFIG_DEBUG)
add_compile_options(-DNDEBUG)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

add_executable(example-cpp-bench cpp_bench.cpp)
target_link_libraries(example-cpp-bench common numa pthread boost_program_options dl)

set_target_properties(example-cpp-bench PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS example-cpp-bench RUNTIME DESTINATION bin)
