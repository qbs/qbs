cmake_minimum_required(VERSION 3.29)
project(dot_in_name VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(dot_in_name)

target_sources(dot_in_name PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    a.module.cppm
)
target_sources(dot_in_name PRIVATE
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(dot_in_name PRIVATE "-fmodules-ts")
endif()
