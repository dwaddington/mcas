cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(personality-python-numpy CXX)

find_program(PYTHON NAMES "python3.7" "python3.6" "python3.5")


if (PYTHON AND BUILD_PYTHON_SUPPORT)

  message("-- Building Python support.")

  set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

  execute_process ( COMMAND ${PYTHON} -m site --user-site OUTPUT_VARIABLE PYTHON_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_paths as gp; print(gp()['include'])" OUTPUT_VARIABLE PYTHON_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_config_vars as gc; print(gc()['LIBPL'])" OUTPUT_VARIABLE PYTHON_LIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_config_vars as gc; print(gc()['BLDLIBRARY'])" OUTPUT_VARIABLE PYTHON_BLDLIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

  message("-- Python    site : ${PYTHON_SITE_PACKAGES}")
  message("--         stdlib : ${PYTHON_LIB_DIR}")
  message("--        include : ${PYTHON_INCLUDE_DIR}")
  set(CMAKE_CXX_STANDARD 14)

  find_program(FLATC NAMES flatc PATHS "${CMAKE_INSTALL_PREFIX}/bin")

  if (NOT FLATC)
    message(FATAL_ERROR ": Flatc compiler not found")
  endif()

  #set(CLIENT_SOURCES ./generated/python_numpy_proto.h ./src/python_numpy_client.cpp)
  set(PLUGIN_SOURCES ./src/python_numpy_plugin.cpp)
  #set(TEST_SOURCES ./src/python_numpy_test.cpp)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/common/include)
  include_directories(${CMAKE_SOURCE_DIR}/src/components)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/flatbuffers/include)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/libnupm/include)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/include)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/libccpm/include)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/libpmem/common)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/libadoproto/include)
  include_directories(${PYTHON_INCLUDE_DIR})

  link_directories(${PYTHON_LIB_DIR})
  link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}) 
  
  add_compile_options("$<$<CONFIG:Debug>:-O0>")

  #add_custom_command(
  #  OUTPUT ./generated/python_numpy_proto.h
  #  DEPENDS src/python_numpy_proto.fbs
  #  COMMAND ${FLATC} --cpp -o ${CMAKE_CURRENT_SOURCE_DIR}/generated/ -c ${CMAKE_CURRENT_SOURCE_DIR}/src/python_numpy_proto.fbs )

  add_definitions(${GCC_COVERAGE_COMPILE_FLAGS} ${FLAG_DUMP_CLASS} -DCONFIG_DEBUG)
  add_compile_options(-g -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls -Wshadow -Wtype-limits -Wno-unused-parameter -Wwrite-strings)

  add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})
  #add_library(${PROJECT_NAME}-client SHARED ${CLIENT_SOURCES})
  #add_executable(${PROJECT_NAME}-test ${TEST_SOURCES})

  target_link_libraries(${PROJECT_NAME} common pthread numa dl rt ${PYTHON_BLDLIB_DIR})

  set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

  install (TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

  configure_file(python-numpy.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/python-numpy.conf)

  #execute_process(COMMAND mkdir -p ${PYTHON_USER_SITE})

  install(DIRECTORY packages/pymcas DESTINATION ${PYTHON_SITE_PACKAGES}/)
  #message("FOO ${Python_SITEARCH}/mcas")
  
  install(DIRECTORY ./
    DESTINATION testing
    FILES_MATCHING PATTERN "*.conf"
    PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)


else()
  message("-- NOT building numpy_python plugin")
endif()
