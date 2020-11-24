cmake_minimum_required(VERSION 3.5)

option(FORCE_OTT_BUILD "force out of tree build" ON)
option(BOOTSTRAP_DEPS "bootstrap to build deps" ON)
option(TEST_HSTORE_PERISHABLE "hstore perishable testing enabled" OFF)
option(BUILD_RUST "enabled Rust-based dependencies" ON)
option(BUILD_KERNEL_SUPPORT "build kernel module dependencies" OFF)
option(BUILD_EXAMPLES_PMDK "build PMDK-based ADO examples (requires PMDK to be installed)" OFF)
option(CODE_COVERAGE "enable gcc code coverage" OFF)

set(DM_REGION_LOG_GRAIN_SIZE 25 CACHE STRING "log base 2 of the device_manager allocation granularity")

if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR AND FORCE_OTT_BUILD)
  message(FATAL_ERROR "Cannot use in-source build ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}. You should delete any CMakeCache.txt and CMakeFiles and then try out-of-tree build")
endif(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR AND FORCE_OTT_BUILD)

# check for Rust installation
find_program(RUST_COMPILER rustc ~/.cargo/bin/)
find_program(RUST_CARGO cargo ~/.cargo/bin/)
if(NOT ${RUST_COMPILER} MATCHES "RUST_COMPILER-NOTFOUND")
  message("-- Found Rust compiler: ${RUST_COMPILER}")
  message("-- Found Rust Cargo: ${RUST_CARGO}")
else()
  set(BUILD_RUST OFF)
endif()

#add_compile_options(-shared-libgcc) # for exception support when using gcc (not g++) to link
add_compile_options(-g)
add_definitions(-DTBB_USE_THREADING_TOOLS -DEASTL_STD_ITERATOR_CATEGORY_ENABLED)

set(ENV{MCAS_HOME} ${CMAKE_CURRENT_SOURCE_DIR})

include(CheckIncludeFiles)
find_package(PkgConfig)
include(${CMAKE_CURRENT_SOURCE_DIR}/mk/common.cmake)
project(comanche)

# locate FindXXX.cmake
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mk)
message("CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# run 'make bootstrap' to  make comanche deps and core
# then run 'make' to build components/tests
add_custom_target(bootstrap
  bash ${CMAKE_SOURCE_DIR}/mk/bootstrap.sh ${CMAKE_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

# bootstrap build should be performed one off
add_subdirectory(src)
add_subdirectory(testing)
add_subdirectory(examples)
add_subdirectory(deploy)


