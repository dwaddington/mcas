cmake_minimum_required(VERSION 3.5)

#configure_file(mapstore-basic.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore-ado0.conf)

install(DIRECTORY ./
  DESTINATION conf
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)
