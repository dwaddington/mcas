cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )

project (kvstore-perf)

add_subdirectory(unit_test)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(GTEST_LIB gtest)
else()
  set(GTEST_LIB gtestd)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include) # rapidjson

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) # tbbmalloc
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # tbbmalloc
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)

add_definitions(-DCONFIG_DEBUG)

add_executable(kvstore-perf kvstore_perf.cpp exp_erase.cpp exp_throughput.cpp experiment.cpp exp_update.cpp program_options.cpp statistics.cpp)

if( ${ARCHITECTURE} STREQUAL "ppc64le" )
  target_link_libraries(kvstore-perf common numa ${GTEST_LIB} pthread dl boost_program_options ${TBB_LIBRARIES} boost_system boost_date_time boost_filesystem tbbmalloc)
else()
  target_link_libraries(kvstore-perf common numa ${GTEST_LIB} pthread dl boost_program_options ${TBB_LIBRARIES} profiler boost_system boost_date_time boost_filesystem tbbmalloc)
endif()

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}:${CMAKE_INSTALL_PREFIX}/lib:${CMAKE_INSTALL_PREFIX}/lib64)

install(TARGETS kvstore-perf RUNTIME DESTINATION bin)
