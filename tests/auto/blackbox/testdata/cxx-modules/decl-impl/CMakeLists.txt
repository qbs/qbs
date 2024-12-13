cmake_minimum_required(VERSION 3.29)
project(declaration_implementation VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(declaration_implementation)

target_sources(declaration_implementation PRIVATE
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    hello.cppm
)
target_sources(declaration_implementation PRIVATE
  hello.cpp
  main.cpp
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(declaration_implementation PRIVATE "-fmodules-ts")
endif()
