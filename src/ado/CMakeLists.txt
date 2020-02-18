cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/private")
  add_subdirectory(private)
else()
  message("-- Not building ADO/private")
  message("-- Private access clone: git clone ")
endif()



