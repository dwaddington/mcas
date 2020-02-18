cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


configure_file(mapstore-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore-ado0.conf)
configure_file(mapstore-secure0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore-secure0.conf)
configure_file(hstore-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-ado0.conf)
configure_file(hstore-cc-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore-cc-ado0.conf)
configure_file(testing-ado0.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/testing-ado0.conf)
configure_file(testing-ado0-sock.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/testing-ado0-sock.conf)

install(FILES "run-tests.sh"
  DESTINATION testing
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_WRITE WORLD_READ)

install(DIRECTORY ./
  DESTINATION testing
  FILES_MATCHING PATTERN "*.sh"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE GROUP_READ GROUP_WRITE WORLD_EXECUTE WORLD_READ WORLD_WRITE)

install(DIRECTORY ./
  DESTINATION testing
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)


