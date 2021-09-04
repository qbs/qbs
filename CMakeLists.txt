cmake_minimum_required(VERSION 3.10)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(VERSION 3.15)
endif()

## Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FeatureSummary)
include(QbsBuildConfig)
include(QbsDocumentation)

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

find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Script QUIET)

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
  if (NOT QBS_USE_BUNDLED_QT_SCRIPT AND Qt5Script_FOUND)
      add_library(qbsscriptengine INTERFACE)
      target_link_libraries(qbsscriptengine INTERFACE Qt5::Script)
  endif()
endif()

if (QBS_INSTALL_HTML_DOCS OR QBS_INSTALL_QCH_DOCS)
    find_package(Python3 COMPONENTS Interpreter)
    if (Qt6_FOUND)
        find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Tools REQUIRED)
    else()
        find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Help REQUIRED)
    endif()
endif()

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(share)
if(WITH_TESTS)
    add_subdirectory(tests)
endif()
