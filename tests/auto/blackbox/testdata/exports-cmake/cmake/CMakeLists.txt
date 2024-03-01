cmake_minimum_required(VERSION 3.10)

project(qbs_import)

find_package(Bar PATHS REQUIRED)
add_executable(Consumer main.cpp)
target_link_libraries(Consumer Bar)
