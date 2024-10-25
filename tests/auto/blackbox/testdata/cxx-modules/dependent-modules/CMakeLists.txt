cmake_minimum_required(VERSION 3.29)
project(dependent_modules VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(dependent_modules)

target_sources(dependent_modules PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    a.cppm
    b.cppm
    c.cppm
)
target_sources(dependent_modules PRIVATE
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(dependent_modules PRIVATE "-fmodules-ts")
endif()
