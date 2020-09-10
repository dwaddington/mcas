cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mcas-extra-personalities")
  add_subdirectory(mcas-extra-personalities)
endif()
