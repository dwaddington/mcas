cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


configure_file(mapstore.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/mapstore.conf)
configure_file(hstore.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/hstore.conf)
