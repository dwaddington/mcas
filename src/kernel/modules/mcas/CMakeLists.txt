cmake_minimum_required(VERSION 3.5)

project(mcas-kernel-module)

if(BUILD_KERNEL_SUPPORT)

  add_custom_target(mcasmod.ko ALL
    COMMENT "Building MCAS kernel module..."
    )

  add_custom_command(TARGET mcasmod.ko
    COMMAND make clean COMMAND make COMMAND make PREFIX=${CMAKE_INSTALL_PREFIX} install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    )

endif(BUILD_KERNEL_SUPPORT)









