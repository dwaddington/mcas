cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(example-python-basic)

configure_file(example-python-basic.py.in ${CMAKE_CURRENT_BINARY_DIR}/example-python-basic.py)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/example-python-basic.py
  DESTINATION bin/
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
