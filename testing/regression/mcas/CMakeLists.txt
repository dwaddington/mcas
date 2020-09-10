cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


# Obsolete; Python-generated configs use install_prefix.py
# configure_file(mapstore-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore-ado0.conf)
# configure_file(mapstore-secure0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore-secure0.conf)
# configure_file(hstore-devdax-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-devdax-ado0.conf)
# configure_file(hstore-fsdax-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-fsdax-ado0.conf)
# configure_file(hstore-cc-devdax-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-cc-devdax-ado0.conf)
# configure_file(hstore-cc-fsdax-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-cc-fsdax-ado0.conf)
# configure_file(hstore-devdax-ado0-sock.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-devdax-ado0-sock.conf)
# configure_file(hstore-fsdax-ado0-sock.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-fsdax-ado0-sock.conf)

configure_file(install_prefix.py.in ${CMAKE_CURRENT_SOURCE_DIR}/install_prefix.py)

install(FILES "run-tests.sh"
  DESTINATION testing
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)

install(DIRECTORY ./
  DESTINATION testing
  FILES_MATCHING PATTERN "*.sh"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)

# Obsolete
# install(DIRECTORY ./
#  DESTINATION testing
#  FILES_MATCHING PATTERN "*.conf"
#  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)

install(DIRECTORY ./
  DESTINATION testing
  FILES_MATCHING PATTERN "*.py"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)


