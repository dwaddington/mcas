cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(mcas-mpi-bench)

enable_language(CXX)

add_compile_options(-DCONFIG_DEBUG)

find_package(MPI 
  COMPONENTS C
  OPTIONAL_COMPONENTS Fortran)



if ('${MPI_C_COMPILER}' MATCHES 'MPI_C_COMPILER-NOTFOUND')
  # let's try again - this is not ideal
  file(GLOB_RECURSE MPI_C_COMPILER /usr/mpi/mpicc)
  file(GLOB_RECURSE MPI_C_LIBRARIES /usr/mpi/libmpi.so)
  file(GLOB_RECURSE MPI_H /usr/mpi/mpi.h)
  if(NOT '${MPI_H}' MATCHES '')
    get_filename_component(MPI_C_INCLUDE_PATH ${MPI_H} DIRECTORY)
    set(MPI_C_FOUND TRUE)
  endif()
endif()

if (MPI_C_FOUND)

message("-- Found MPI compiler: ${MPI_C_COMPILER}")
message("-- Found MPI include path: ${MPI_C_INCLUDE_PATH}")
message("-- Found MPI libraries: ${MPI_C_LIBRARIES}")

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_INSTALL_PREFIX}/include)
include_directories(${MPI_C_INCLUDE_PATH})

link_directories(${CMAKE_BINARY_DIR}/src/lib/common)
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
link_directories(${CMAKE_INSTALL_PREFIX}/lib64)
link_directories(${MPI_C_LIBRARIES})

add_executable(${PROJECT_NAME} mcas_mpi_bench.cpp)

target_link_libraries(${PROJECT_NAME} common numa pthread boost_program_options dl ${MPI_C_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib:${MPI_HOME}/lib64::${MPI_HOME}/lib)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

endif() # MPI_C_FOUND

