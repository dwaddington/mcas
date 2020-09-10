cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

add_subdirectory(demo)
add_subdirectory(passthru)
add_subdirectory(testing)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/enterprise-only")
  add_subdirectory(enterprise-only)
endif()



