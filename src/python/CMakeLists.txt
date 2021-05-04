cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

if (BUILD_PYTHON_SUPPORT)
  
add_subdirectory(mcas-python-api)
add_subdirectory(python-personality)
add_subdirectory(pymm)

endif(BUILD_PYTHON_SUPPORT)


