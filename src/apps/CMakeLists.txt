cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wformat=2 -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings)

add_subdirectory(kvstore-perf)
add_subdirectory(ado-perf)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ycsb-perf")
  add_subdirectory(ycsb-perf)
endif()

