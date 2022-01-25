add_library(span INTERFACE)
target_include_directories(
    span
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
