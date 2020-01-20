cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(mini-etcd CXX)

find_program(PROTOC NAMES protoc)

if (PROTOC) # build only when protoc is available

set (CMAKE_CXX_STANDARD 14)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/mini.pb.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mini.proto
  COMMAND ${PROTOC} -I${CMAKE_CURRENT_SOURCE_DIR} 
    --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mini.proto
)

set(CLIENT_LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mini.pb.h client.cpp)

add_library(${PROJECT_NAME} SHARED ${CLIENT_LIB_SOURCES})

#target_link_libraries(${PROJECT_NAME} common numa dl rt boost_system pthread curl flatbuffers)

endif()
