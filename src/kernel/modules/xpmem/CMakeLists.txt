cmake_minimum_required(VERSION 3.5)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if(DEFINED SKIP_KERNEL_MODULES)
  ExternalProject_Add(xpmem-kernel-module
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/kernel/modules/xpmem
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
    COMMAND ./configure --disable-kernel-module --prefix=${CMAKE_INSTALL_PREFIX}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    )
else(DEFINED SKIP_KERNEL_MODULES)
  ExternalProject_Add(xpmem-kernel-module
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/kernel/modules/xpmem
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
    COMMAND ./configure --with-kerneldir=/lib/modules/${CMAKE_HOST_SYSTEM_VERSION}/build --prefix=${CMAKE_INSTALL_PREFIX}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    )
endif(DEFINED SKIP_KERNEL_MODULES)

