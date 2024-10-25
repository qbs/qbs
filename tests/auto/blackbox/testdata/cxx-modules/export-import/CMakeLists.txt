cmake_minimum_required(VERSION 3.29)
project(export_import VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(export_import)

target_sources(export_import PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    a.cppm
    b.cppm
    c.cppm
)
target_sources(export_import PRIVATE
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(export_import PRIVATE "-fmodules-ts")
endif()
