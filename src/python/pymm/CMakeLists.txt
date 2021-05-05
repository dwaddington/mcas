cmake_minimum_required(VERSION 3.5) # may work with earlier versions

project(pymm CXX)

option(BUILD_PYTHON_SUPPORT "Build Python support" OFF)

if (PYTHON AND BUILD_PYTHON_SUPPORT)

  # this is the directory for where the package will be constructed
  #
  set(PKG_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pymm")
  
  find_program(PYTHON NAMES "python3.7" "python3.6" "python3.5")
  
  get_filename_component(PYTHON_FILENAME ${PYTHON} NAME)
  # get python locations
  execute_process ( COMMAND ${PYTHON} -m site --user-site OUTPUT_VARIABLE PYTHON_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_paths as gp; print(gp()['include'])" OUTPUT_VARIABLE PYTHON_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_config_vars as gc; print(gc()['LIBPL'])" OUTPUT_VARIABLE PYTHON_LIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process ( COMMAND ${PYTHON} -c "from sysconfig import get_config_vars as gc; print(gc()['BLDLIBRARY'])" OUTPUT_VARIABLE PYTHON_BLDLIB_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
  
  message("-- Python    site : ${PYTHON_SITE_PACKAGES}")
  message("--         stdlib : ${PYTHON_LIB_DIR}")
  message("--        include : ${PYTHON_INCLUDE_DIR}")
  
  message("-- Building Python support.")
  set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
  set(SETUP_PY "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
  set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py")
  set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/timestamp")
  
  string(REGEX REPLACE  "^.*python" "" PYTHON_VERSION ${PYTHON})
  
  message("-- Python version ${PYTHON_VERSION}")
  set(LIBDIR "lib.linux-x86_64-${PYTHON_VERSION}")
  set(PYTHON_SITE_NUMPY_INCLUDES "${PYTHON_SITE_PACKAGES}/numpy/core/include")

  configure_file(${SETUP_PY_IN} ${SETUP_PY})

  file(GLOB PACKAGE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.py ${CMAKE_CURRENT_SOURCE_DIR}/core/*.h ${CMAKE_CURRENT_SOURCE_DIR}/core/*.c* )
  # copy python files to build
  
  file(MAKE_DIRECTORY ${PKG_SOURCE_DIR})
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION ${PKG_SOURCE_DIR})
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/pymm.py DESTINATION ${PKG_SOURCE_DIR})
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py DESTINATION ${PKG_SOURCE_DIR})
  
  add_custom_command(
    OUTPUT build.stamp
    COMMAND ${PYTHON} ${SETUP_PY} bdist # build wheel binary distribution    
    COMMAND cmake -E touch build.stamp
    DEPENDS ${PACKAGE_SOURCE_FILES} 
    )

  add_custom_target(${PROJECT_NAME} ALL
    DEPENDS build.stamp
    )
  
    

  # install as a site-package
  install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --user)")
  
  
else()
  message("-- NOT building Python support.")
endif()

