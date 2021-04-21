cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 14)

project(component-mm-passthru CXX)

add_definitions(-DCONFIG_DEBUG)

include(../../../../mk/clang-dev-tools.cmake)

include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
include_directories(${CMAKE_SOURCE_DIR}/src/components)
include_directories(${CMAKE_SOURCE_DIR}/src/lib/libmm/src)
include_directories(${CMAKE_INSTALL_PREFIX}/include)
  
file(GLOB SOURCES src/mm_passthru.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_compile_options(${PROJECT_NAME} PUBLIC "-fPIC")

# see https://flameeyes.blog/2012/10/07/symbolism-and-elf-files-or-what-does-bsymbolic-do/
#
# -Bsymbolic and -Bsymbolic-functions effectively prevent the PLT
# (Procedure Linking Table) from being used to resolve the symbol and
# stick to the internally provided symbol
#
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic -Wl,-Bsymbolic-functions -Wl,--wrap=malloc \
-Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=memalign -Wl,--wrap=fprintf") # leave undefined

# see https://libcxxabi.llvm.org/spec.html


target_link_libraries(${PROJECT_NAME} -static-libgcc -static-libstdc++ common-static dl)

# set the linkage in the install/lib
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install(TARGETS ${PROJECT_NAME}  LIBRARY DESTINATION lib)


