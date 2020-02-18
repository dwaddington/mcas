cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)


project(fabric-tests CXX)

include_directories(../../../../components)
include_directories(../../../../lib/common/include/)
link_directories(${CMAKE_INSTALL_PREFIX}/lib) # libfabric

add_executable(fabric-test1 test1.cpp
  eyecatcher.cpp
  mr_lock.cpp
  patience.cpp
  pingpong_cb_ctxt.cpp
  pingpong_cb_pack.cpp
  pingpong_client.cpp
  pingpong_cnxn_state.cpp
  pingpong_server.cpp
  pingpong_server_cb.cpp
  pingpong_server_client_state.cpp
  pingpong_server_n.cpp
  registered_memory.cpp
  registration.cpp
  remote_memory_accessor.cpp
  remote_memory_client.cpp
  remote_memory_client_for_shutdown.cpp
  remote_memory_client_grouped.cpp
  remote_memory_server.cpp
  remote_memory_server_grouped.cpp
  remote_memory_subclient.cpp
  remote_memory_subserver.cpp
  server_connection_and_memory.cpp
  server_connection.cpp
  server_grouped_connection.cpp
  wait_poll.cpp
)

target_link_libraries(fabric-test1 ${ASAN_LIB} common numa fabric pthread gtest dl)
