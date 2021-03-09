cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(store-tests CXX)

include_directories(../../)
include_directories(../../../)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/GSL/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)

add_definitions(-DCONFIG_DEBUG)
add_definitions(-DDM_REGION_LOG_GRAIN_SIZE=${DM_REGION_LOG_GRAIN_SIZE})
add_executable(${PROJECT_NAME} src/main.cpp)
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_link_libraries(${PROJECT_NAME} ${ASAN_LIB} common numa pthread dl boost_program_options)

add_executable(dax_address_overlap src/dax_address_overlap.cpp)
target_compile_options(dax_address_overlap PUBLIC "$<$<CONFIG:Debug>:-O0>")
target_link_libraries(dax_address_overlap ${ASAN_LIB} common numa pthread dl boost_program_options)
