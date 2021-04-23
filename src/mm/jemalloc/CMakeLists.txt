cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(mm-plugin-jemalloc C CXX)

include_directories(${CMAKE_BINARY_DIR}/src/mm/jemalloc-dist/include)
set(SOURCES src/mm_plugin_jemalloc.cpp)

add_compile_options(-pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
link_directories(${CMAKE_BINARY_DIR}/src/mm/jemalloc-dist/lib)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic -Wl,-Bsymbolic-functions -Wl,--wrap=malloc \
-Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=memalign")

target_compile_options(${PROJECT_NAME} PUBLIC "-O0")
target_link_libraries(${PROJECT_NAME} -static-libgcc -static libjemalloc_pic.a pthread)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
