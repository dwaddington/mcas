cmake_minimum_required(VERSION 3.5) # may work with earlier versions

project(mcas-python-api CXX)

option(BUILD_PYTHON_SUPPORT "Build Python support" OFF)

find_program(PYTHON NAMES "python3.7" "python3.6" "python3.5")

if (PYTHON AND BUILD_PYTHON_SUPPORT)

  get_filename_component(PYTHON_FILENAME ${PYTHON} NAME)

  message("-- Building Python support.")
  set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
  set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
  set(CONFIG_H_IN "${CMAKE_CURRENT_SOURCE_DIR}/src/mcas_python_config.h.in")
  set(CONFIG_H    "${CMAKE_CURRENT_BINARY_DIR}/src/mcas_python_config.h")

  set(DEPS        "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py")
  set(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")
  string(REGEX REPLACE  "^.*python" "" PYTHON_VERSION ${PYTHON})
  message("-- Python version ${PYTHON_VERSION}")
  set(LIBDIR      "lib.linux-x86_64-${PYTHON_VERSION}")

  configure_file(${SETUP_PY_IN} ${SETUP_PY})
  configure_file(${CONFIG_H_IN} ${CONFIG_H})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mcas-shell.in ${CMAKE_CURRENT_SOURCE_DIR}/mcas-shell)

  # copy python files to build
  set(PKG_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/mcas_pkg")
  file(MAKE_DIRECTORY ${PKG_SOURCE_DIR})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LICENSE ${CMAKE_CURRENT_BINARY_DIR}/LICENSE)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mcasapi.py ${PKG_SOURCE_DIR}/mcasapi.py COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/__init__.py ${PKG_SOURCE_DIR}/__init__.py COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/normal_start.py ${PKG_SOURCE_DIR}/normal_start.py COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.py ${PKG_SOURCE_DIR}/test.py COPYONLY)
 
  set(DEBUG_COMMAND "${LD_PATH}" "PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_BINARY_DIR}/build/${LIBDIR}" gdb --args "${PYTHON}" ${PYTHON_FLAGS} -i "${CMAKE_CURRENT_SOURCE_DIR}/normal_start.py")
  set(CHECK_COMMAND "${LD_PATH}" "PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_CURRENT_BINARY_DIR}/build/${LIBDIR}" "${PYTHON}" ${PYTHON_FLAGS} -i "${CMAKE_CURRENT_SOURCE_DIR}/normal_start.py")

  add_custom_command(OUTPUT ${OUTPUT}
    COMMAND ${PYTHON} ${SETUP_PY} build
    COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
    DEPENDS ${DEPS})

  add_custom_target(${PROJECT_NAME}-target ALL DEPENDS ${OUTPUT})
  add_custom_target(${PROJECT_NAME}-debug COMMAND ${DEBUG_COMMAND})
  add_custom_target(${PROJECT_NAME}-check COMMAND ${CHECK_COMMAND})
  add_custom_target(${PROJECT_NAME}-wheel COMMAND ${PYTHON} ${SETUP_PY} bdist) # build wheel binary distribution

  # install as a site-package
  install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --user)")
  
  install(FILES mcas-shell DESTINATION ${CMAKE_INSTALL_PREFIX}/bin PERMISSIONS
    OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ)
  
  install(FILES mcas-shell-init.py DESTINATION ${CMAKE_INSTALL_PREFIX}/lib PERMISSIONS
    OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ)
  
  install(FILES mcasapi.py DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64/python PERMISSIONS
    OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ)
  
else()
  message("-- NOT building Python support.")
endif()

