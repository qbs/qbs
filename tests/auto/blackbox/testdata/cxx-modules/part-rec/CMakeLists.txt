cmake_minimum_required(VERSION 3.29)
project(partitions VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(partitions)

target_sources(partitions PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    a.p1.cppm
    a.p2.cppm
    a.cppm
    b.p1.cppm
    b.p2.cppm
    b.cppm
)
target_sources(partitions PRIVATE
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(partitions PRIVATE "-fmodules-ts")
endif()
