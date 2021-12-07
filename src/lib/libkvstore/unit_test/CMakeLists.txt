cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project(kvstore-wrapper-test C)

link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

add_compile_options(-g -fPIC)

add_executable(${PROJECT_NAME} test1.c)
target_link_libraries(${PROJECT_NAME} common numa dl kvstore)

add_executable(${PROJECT_NAME}-2 test2.c)
target_link_libraries(${PROJECT_NAME}-2 common numa dl kvstore)
