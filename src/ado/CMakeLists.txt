cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

find_program(RUSTC rustc)

add_subdirectory(demo)
add_subdirectory(passthru)
add_subdirectory(testing)

if(EXISTS ${RUSTC})
  add_subdirectory(rust-wrapper)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/enterprise-only")
  add_subdirectory(enterprise-only)
endif()



