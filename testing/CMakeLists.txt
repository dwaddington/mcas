cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

if(BOOTSTRAP_DEPS)
else(BOOTSTRAP_DEPS)
add_subdirectory(libnop)
add_subdirectory(regression)
add_subdirectory(ado-test)
add_subdirectory(kv-test)
add_subdirectory(eastl-test)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/mcas-testing")
  add_subdirectory(mcas-testing)
endif()

endif(BOOTSTRAP_DEPS)
