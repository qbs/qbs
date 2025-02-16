cmake_minimum_required(VERSION 3.15)
project(conanfileprobe.testlib)

add_library(${PROJECT_NAME} STATIC testlib.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "testlib.h")
install(TARGETS ${PROJECT_NAME})
