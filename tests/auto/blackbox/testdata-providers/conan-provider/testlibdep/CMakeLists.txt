cmake_minimum_required(VERSION 3.15)
project(conanmoduleprovider.testlibdep)

add_library(${PROJECT_NAME} STATIC testlibdep.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "testlibdep.h")
install(TARGETS ${PROJECT_NAME})
install(FILES lorem_ipsum.txt DESTINATION share)