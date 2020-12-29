cmake_minimum_required (VERSION 3.5.1 FATAL_ERROR)

project (eastl-example)

include(${CMAKE_SOURCE_DIR}/mk/avoid_boost_type_to_string_bug.cmake)

include_directories(${CMAKE_INSTALL_PREFIX}/include)

add_compile_options(-g -pedantic -Wall -Wextra -Werror -Wcast-align -Wcast-qual -Wconversion -Weffc++ -Wold-style-cast -Wredundant-decls -Wshadow -Wtype-limits -Wunused-parameter -Wwrite-strings -Wformat=2)

set(SOURCES src/main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} dl)

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-O0>")
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
