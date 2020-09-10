cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

if(DEFINED BUILD_KERNEL_SUPPORT)
  add_subdirectory(mcas)
  add_subdirectory(xpmem)
endif(DEFINED BUILD_KERNEL_SUPPORT)
  


