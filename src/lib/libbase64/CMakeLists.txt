cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(base64)

add_compile_options(-std=c99 -O3 -Wall -Wextra -pedantic -mavx -mavx2 -msse4)

set(SOURCES lib/arch/avx2/codec.c 
  lib/arch/generic/codec.c 
  lib/arch/ssse3/codec.c 
  lib/arch/sse41/codec.c 
  lib/arch/sse42/codec.c 
  lib/arch/avx/codec.c 
  lib/lib.c 
  lib/codec_choose.c 
  lib/tables/tables.c)


add_library(base64 SHARED ${SOURCES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/  DESTINATION include FILES_MATCHING PATTERN "*.h*")
