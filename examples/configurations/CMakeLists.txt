cmake_minimum_required(VERSION 3.5)

file(GLOB files "*.conf.in")
foreach(file ${files})
  get_filename_component(BASEFILE ${file} NAME)
  get_filename_component(TARGETFILE ${file} NAME_WLE)
  configure_file(${BASEFILE} ${TARGETFILE})
endforeach()

# configure_file(example-mapstore-ado0.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-mapstore-ado0.conf)
# configure_file(example-hstore-devdax-secure-0.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-hstore-devdax-secure-0.conf)
# configure_file(example-mapstore-rust-ado.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-mapstore-rust-ado.conf)
# configure_file(example-hstore-rust-ado.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-hstore-rust-ado.conf)
# configure_file(example-mapstore-adotest0.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-mapstore-adotest0.conf)
# configure_file(example-hstore-pmem-ado0.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-hstore-pmem-ado0.conf)
# configure_file(example-hstore-devdax-ado0.conf.in ${CMAKE_CURRENT_BINARY_DIR}/example-hstore-devdax-ado0.conf)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
  DESTINATION conf
  FILES_MATCHING PATTERN "*.conf"
  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE GROUP_WRITE WORLD_WRITE)
