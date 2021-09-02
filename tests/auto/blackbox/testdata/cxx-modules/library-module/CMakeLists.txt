cmake_minimum_required(VERSION 3.29)
project(library_module VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(library_module_lib SHARED)

target_sources(library_module_lib PUBLIC
  FILE_SET all_my_modules TYPE CXX_MODULES
  BASE_DIRS
    ${PROJECT_SOURCE_DIR}
  FILES
    lib/hello.cppm
)

add_executable(library_module_app)

target_sources(library_module_app PRIVATE
  app/main.cpp
)
target_link_libraries(library_module_app library_module_lib)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(library_module_lib PRIVATE "-fmodules-ts")
  target_compile_options(library_module_app PRIVATE "-fmodules-ts")
endif()
