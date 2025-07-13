cmake_minimum_required(VERSION 3.15)
project(lib_order_test CXX)

add_library(C STATIC C/C.cpp)
add_library(B STATIC B/B.cpp)
add_library(A STATIC A/A.cpp)

target_link_libraries(B PUBLIC C)
target_link_libraries(A PUBLIC B)

install(TARGETS A B C
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
