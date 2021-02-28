cmake_minimum_required(VERSION 3.15)
project(conanmoduleprovider.testlib)

find_package(conanmoduleprovider.testlibdep REQUIRED)

add_library(${PROJECT_NAME} STATIC testlib.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "testlib.h")
target_link_libraries(${PROJECT_NAME} conanmoduleprovider.testlibdep::conanmoduleprovider.testlibdep)
install(TARGETS ${PROJECT_NAME})
