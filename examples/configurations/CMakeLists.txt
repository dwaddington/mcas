cmake_minimum_required(VERSION 3.5)

file(GLOB files "*.conf.in")
foreach(file ${files})
  get_filename_component(BASEFILE ${file} NAME)
  get_filename_component(TARGETFILE ${file} NAME_WE)
  configure_file(${BASEFILE} ${TARGETFILE}.conf)
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
  DESTINATION conf
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)
