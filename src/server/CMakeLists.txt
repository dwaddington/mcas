cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wdeprecated-copy-dtor COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings $<$<BOOL:${COMPILER_SUPPORTS_DEPRECATED_COPY_DTOR}>:-Wdeprecated-copy-dtor>)

if(BOOTSTRAP_DEPS)
else(BOOTSTRAP_DEPS)

if(BUILD_MCAS_SERVER)
  add_subdirectory(mcas)
  add_subdirectory(ado)
endif()

endif(BOOTSTRAP_DEPS)

