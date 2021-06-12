cmake_minimum_required(VERSION 3.15)

## Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FeatureSummary)
include(QbsBuildConfig)

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

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)

if(WITH_TESTS)
    enable_testing()
    set(QT_TEST_COMPONENT Test)
    set(IMPLICIT_DEPENDS Qt${QT_VERSION_MAJOR}::Test)
endif()

find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS Concurrent Core Gui Network Widgets Xml ${QT_TEST_COMPONENT}
    REQUIRED
    )

find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Script)

if (Qt6_FOUND)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core5Compat REQUIRED)
    if(NOT TARGET Qt6Core5Compat)
        set_property(TARGET Qt6::Core5Compat PROPERTY IMPORTED_GLOBAL TRUE) # hack for CMake < 1.18
        add_library(Qt6Core5Compat ALIAS Qt6::Core5Compat)
    endif()
else()
  if(NOT TARGET Qt6Core5Compat)
    add_library(Qt6Core5Compat INTERFACE)
  endif()
  if(Qt5Script_FOUND)
      set_property(TARGET Qt5::Script PROPERTY IMPORTED_GLOBAL TRUE)
  endif()
endif()

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(share)
if(WITH_TESTS)
    add_subdirectory(tests)
endif()
