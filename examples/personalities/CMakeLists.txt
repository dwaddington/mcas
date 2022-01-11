cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

add_subdirectory(cpp_versioning)
add_subdirectory(cpp_list)
add_subdirectory(cpp_symtab)
add_subdirectory(tabulator_ccpm_fb)
add_subdirectory(cpp_pm_sort)

if(BUILD_EXAMPLES_PMDK)
  add_subdirectory(tabulator_pmdk_fb)
endif(BUILD_EXAMPLES_PMDK)
