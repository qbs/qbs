cmake_minimum_required(VERSION 3.10)

## Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FeatureSummary)
include(QbsAPI)

file(STRINGS VERSION QBS_VERSION)
project(Qbs VERSION ${QBS_VERSION})

# Force C++ standard, do not fall back, do not use compiler extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set up Qt stuff:
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(WITH_TESTS "Build Tests" ON)
option(WITH_UNIT_TESTS "Build Unit Tests" OFF)
option(WITH_PROJECT_FILE_UPDATES "Enable project file updates support" OFF)
option(INSTALL_PUBLIC_HEADERS "Whether to install public headers" ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) # Less useful to do it for linking, see edit2
endif(CCACHE_FOUND)

if(WITH_TESTS)
    enable_testing()
    set(QT_TEST_COMPONENT Test)
    set(IMPLICIT_DEPENDS Qt5::Test)
endif()

find_package(Qt5
    COMPONENTS Concurrent Core Gui Network Script Widgets Xml ${QT_TEST_COMPONENT}
    REQUIRED
    )

add_subdirectory(src)
add_subdirectory(share)
if(WITH_TESTS)
    add_subdirectory(tests)
endif()
