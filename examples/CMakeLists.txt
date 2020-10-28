cmake_minimum_required(VERSION 3.5)

if(BOOTSTRAP_DEPS)
else(BOOTSTRAP_DEPS)
add_subdirectory(cpp_basic)
add_subdirectory(python_basic)
add_subdirectory(personalities)
add_subdirectory(configurations)
endif(BOOTSTRAP_DEPS)
