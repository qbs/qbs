cmake_minimum_required(VERSION 3.29)
project(single_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(single_module)

target_sources(single_module PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    hello.cppm
)
target_sources(single_module PRIVATE
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(single_module PRIVATE "-fmodules-ts")
endif()
