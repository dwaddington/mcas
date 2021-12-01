cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wdeprecated-copy-dtor COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings $<$<BOOL:${COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR}>:-Wdeprecated-copy-dtor>)

if(BUILD_MCAS_SERVER AND BUILD_MCAS_CLIENT)
  add_subdirectory(kvstore-perf)
  add_subdirectory(ado-perf)

  if(BUILD_MPI_APPS)
    add_subdirectory(mcas-mpi-bench)
  endif(BUILD_MPI_APPS)
endif()

