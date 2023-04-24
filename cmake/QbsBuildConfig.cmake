option(WITH_TESTS "Build Tests" ON)
option(WITH_UNIT_TESTS "Build Unit Tests" OFF)
option(INSTALL_PUBLIC_HEADERS "Whether to install public headers" ON)
option(QBS_ENABLE_RPATH "Whether to enable RPATH" ON)

include(CMakeDependentOption)
cmake_dependent_option(QBS_QUICKJS_LEAK_CHECK "Whether to check for quickjs leaks at the end" ON "CMAKE_BUILD_TYPE STREQUAL Debug" OFF)

set(QBS_APP_INSTALL_DIR "bin" CACHE STRING "Relative install location for Qbs binaries.")
# default paths
set(QBS_LIBDIR_NAME "lib")
if(WIN32)
    set(_DEFAULT_LIB_INSTALL_DIR ${QBS_APP_INSTALL_DIR})
    set(_DEFAULT_LIBEXEC_INSTALL_DIR ${QBS_APP_INSTALL_DIR})
else()
    set(_DEFAULT_LIB_INSTALL_DIR ${QBS_LIBDIR_NAME})
    set(_DEFAULT_LIBEXEC_INSTALL_DIR "libexec/qbs")
endif()

# path options
set(QBS_OUTPUT_PREFIX "" CACHE STRING "Qbs build output location relative to CMAKE_BINARY_DIR.")
set(QBS_LIB_INSTALL_DIR "${_DEFAULT_LIB_INSTALL_DIR}" CACHE STRING "Relative install location for Qbs libraries.")
set(QBS_DLL_INSTALL_DIR "${QBS_LIB_INSTALL_DIR}" CACHE STRING "Relative install location for Qbs DLLs.")
set(QBS_LIBEXEC_INSTALL_DIR "${_DEFAULT_LIBEXEC_INSTALL_DIR}" CACHE STRING "Relative install location for Qbs libexec.")
set(QBS_PLUGINS_INSTALL_BASE "${QBS_LIBDIR_NAME}" CACHE STRING "Relative install location for Qbs plugins.")
set(QBS_RESOURCES_INSTALL_BASE "." CACHE STRING "Relative install location for Qbs resources.")
set(QBS_HEADERS_INSTALL_DIR "include/qbs" CACHE STRING "Relative install location for Qbs headers.")
set(QBS_DOC_INSTALL_DIR "${QBS_RESOURCES_INSTALL_BASE}/share/doc/qbs" CACHE STRING "Relative install location for Qbs documentation.")
set(QBS_DOC_HTML_DIR_NAME "html" CACHE STRING "The name of the dir with HTML files, appended to QBS_DOC_INSTALL_DIR.")

set(QBS_PLUGINS_INSTALL_DIR "${QBS_PLUGINS_INSTALL_BASE}/qbs/plugins")
set(QBS_RESOURCES_INSTALL_DIR "${QBS_RESOURCES_INSTALL_BASE}/share")

# rpaths
file(RELATIVE_PATH QBS_RELATIVE_LIBEXEC_RPATH "/${QBS_LIBEXEC_INSTALL_DIR}" "/${QBS_LIB_INSTALL_DIR}")
file(RELATIVE_PATH QBS_RELATIVE_APP_RPATH "/${QBS_APP_INSTALL_DIR}" "/${QBS_LIB_INSTALL_DIR}")
file(RELATIVE_PATH QBS_RELATIVE_PLUGINS_RPATH "/${QBS_PLUGINS_INSTALL_DIR}" "/${QBS_LIB_INSTALL_DIR}")

if(WIN32 OR NOT QBS_ENABLE_RPATH)
  set(QBS_MACOSX_RPATH OFF)
  set(QBS_LIB_RPATH "")
  set(QBS_LIBEXEC_RPATH "")
  set(QBS_APP_RPATH "")
  set(QBS_PLUGINS_RPATH "")
elseif(APPLE)
    set(QBS_MACOSX_RPATH ON)
    set(QBS_LIB_RPATH "@loader_path")
    set(QBS_LIBEXEC_RPATH "@loader_path/${QBS_RELATIVE_LIBEXEC_RPATH}")
    set(QBS_APP_RPATH "@loader_path/${QBS_RELATIVE_APP_RPATH}")
    set(QBS_PLUGINS_RPATH "@loader_path/${QBS_RELATIVE_PLUGINS_RPATH}")
else()
    set(QBS_MACOSX_RPATH OFF)
    set(QBS_LIB_RPATH "\$ORIGIN")
    set(QBS_LIBEXEC_RPATH "\$ORIGIN/${QBS_RELATIVE_LIBEXEC_RPATH}")
    set(QBS_APP_RPATH "\$ORIGIN/${QBS_RELATIVE_APP_RPATH}")
    set(QBS_PLUGINS_RPATH "\$ORIGIN/${QBS_RELATIVE_PLUGINS_RPATH}")
endif()

function(get_update_path_command var)
    if(WIN32)
        get_target_property(_QTCORE_LIBRARY Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
        if(NOT _QTCORE_LIBRARY)
            get_target_property(_QTCORE_LIBRARY Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_DEBUG)
        endif()
        get_filename_component(_QT_LIBRARY_PATH "${_QTCORE_LIBRARY}" DIRECTORY)
        get_target_property(_QBS_LIBRARY_PATH qbscore LIBRARY_OUTPUT_DIRECTORY)
        file(TO_NATIVE_PATH "${_QT_LIBRARY_PATH}\;${_QBS_LIBRARY_PATH}\;$ENV{PATH}" _NEW_PATH)
        set(${var} "PATH=${_NEW_PATH}" PARENT_SCOPE)
    else()
        set(${var} "")
    endif()
endfunction()

if(WITH_UNIT_TESTS)
    set(QBS_UNIT_TESTS_DEFINES "QBS_ENABLE_UNIT_TESTS")
else()
    set(QBS_UNIT_TESTS_DEFINES "")
endif()

file(RELATIVE_PATH QBS_RELATIVE_LIBEXEC_PATH "/${QBS_APP_INSTALL_DIR}" "/${QBS_LIBEXEC_INSTALL_DIR}")
file(RELATIVE_PATH QBS_RELATIVE_SEARCH_PATH "/${QBS_APP_INSTALL_DIR}" "/${QBS_RESOURCES_INSTALL_BASE}")
file(RELATIVE_PATH QBS_RELATIVE_PLUGINS_PATH "/${QBS_APP_INSTALL_DIR}" "/${QBS_PLUGINS_INSTALL_BASE}")

set(DEFAULT_DEFINES "")
if(WIN32)
  list(APPEND DEFAULT_DEFINES UNICODE _UNICODE _SCL_SECURE_NO_WARNINGS)
endif()
if(WITH_TESTS)
  list(APPEND DEFAULT_DEFINES QBS_WITH_TESTS)
endif()

# CMake 3.10 doesn't have list(TRANSFORM)
function(list_transform_prepend var prefix)
    set(temp "")
    foreach(f ${${var}})
        list(APPEND temp "${prefix}${f}")
    endforeach()
    set(${var} "${temp}" PARENT_SCOPE)
endfunction()

function(add_qbs_app target_name)
    cmake_parse_arguments(_arg
        ""
        "DESTINATION"
        "DEFINES;PUBLIC_DEFINES;DEPENDS;PUBLIC_DEPENDS;INCLUDES;PUBLIC_INCLUDES;SOURCES;"
        ${ARGN}
        )

    if (${_arg_UNPARSED_ARGUMENTS})
        message(FATAL_ERROR "add_qbs_app had unparsed arguments")
    endif()

    set(_DESTINATION "${QBS_APP_INSTALL_DIR}")
    if(_arg_DESTINATION)
      set(_DESTINATION "${_arg_DESTINATION}")
    endif()

    add_executable(${target_name} ${_arg_SOURCES})
    target_compile_definitions(
        ${target_name} PRIVATE ${_arg_DEFINES} ${DEFAULT_DEFINES} PUBLIC ${_arg_PUBLIC_DEFINES})
    target_include_directories(
        ${target_name} PRIVATE ${_arg_INCLUDES} PUBLIC ${_arg_PUBLIC_INCLUDES})
    target_link_libraries(${target_name} PRIVATE ${_arg_DEPENDS} PUBLIC ${_arg_PUBLIC_DEPENDS})

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${_DESTINATION}
        BUILD_RPATH "${QBS_APP_RPATH}"
        INSTALL_RPATH "${QBS_APP_RPATH}"
        MACOSX_RPATH ${QBS_MACOSX_RPATH}
        )
    install(TARGETS ${target_name} RUNTIME DESTINATION ${_DESTINATION})
endfunction()

function(add_qbs_library target_name)
    cmake_parse_arguments(_arg
        "STATIC"
        ""
        "DEFINES;PUBLIC_DEFINES;DEPENDS;PUBLIC_DEPENDS;INCLUDES;PUBLIC_INCLUDES;SOURCES;"
        ${ARGN}
        )

    if (${_arg_UNPARSED_ARGUMENTS})
        message(FATAL_ERROR "add_qbs_library had unparsed arguments")
    endif()

    set(library_type SHARED)
    if (_arg_STATIC)
        set(library_type STATIC)
    endif()

    string(REGEX REPLACE "\\.[0-9]+$" "" _SOVERSION ${QBS_VERSION})

    add_library(${target_name} ${library_type} ${_arg_SOURCES})
    target_compile_definitions(
        ${target_name}
        PRIVATE ${_arg_DEFINES} ${DEFAULT_DEFINES}
        PUBLIC ${_arg_PUBLIC_DEFINES})
    target_include_directories(
        ${target_name}
        PRIVATE ${_arg_INCLUDES}
        PUBLIC ${_arg_PUBLIC_INCLUDES}
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>)
    target_link_libraries(${target_name} PRIVATE ${_arg_DEPENDS} PUBLIC ${_arg_PUBLIC_DEPENDS})

    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_LIB_INSTALL_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_LIB_INSTALL_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_DLL_INSTALL_DIR}
        BUILD_RPATH "${QBS_LIB_RPATH}"
        INSTALL_RPATH "${QBS_LIB_RPATH}"
        MACOSX_RPATH ${QBS_MACOSX_RPATH}
        SOVERSION ${_SOVERSION}
        VERSION ${QBS_VERSION}
        )
    if (NOT _arg_STATIC)
        install(TARGETS ${target_name}
            LIBRARY DESTINATION ${QBS_LIB_INSTALL_DIR}
            RUNTIME DESTINATION ${QBS_DLL_INSTALL_DIR}
        )
    endif()
    if(MSVC)
      target_compile_options(${target_name} PUBLIC /EHsc)
    endif()
endfunction()

function(add_qbs_plugin target_name)
    cmake_parse_arguments(_arg
        ""
        ""
        "DEFINES;PUBLIC_DEFINES;DEPENDS;PUBLIC_DEPENDS;INCLUDES;PUBLIC_INCLUDES;SOURCES;"
        ${ARGN}
        )

    if (${_arg_UNPARSED_ARGUMENTS})
        message(FATAL_ERROR "add_qbs_plugin had unparsed arguments")
    endif()

    add_library(${target_name} SHARED ${_arg_SOURCES})
    target_compile_definitions(
        ${target_name} PRIVATE ${_arg_DEFINES} ${DEFAULT_DEFINES} PUBLIC ${_arg_PUBLIC_DEFINES})
    target_include_directories(
        ${target_name} PRIVATE ${_arg_INCLUDES} PUBLIC ${_arg_PUBLIC_INCLUDES})
    target_link_libraries(${target_name} PRIVATE ${_arg_DEPENDS} PUBLIC ${_arg_PUBLIC_DEPENDS})

    set_target_properties(${target_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_PLUGINS_INSTALL_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_PLUGINS_INSTALL_DIR}
        BUILD_RPATH "${QBS_PLUGINS_RPATH}"
        INSTALL_RPATH "${QBS_PLUGINS_RPATH}"
        MACOSX_RPATH ${QBS_MACOSX_RPATH}
        )
    install(TARGETS ${target_name}
        LIBRARY DESTINATION ${QBS_PLUGINS_INSTALL_DIR}
        RUNTIME DESTINATION ${QBS_PLUGINS_INSTALL_DIR}
    )
endfunction()

function(add_qbs_test test_name)
    cmake_parse_arguments(_arg
        ""
        ""
        "DEFINES;PUBLIC_DEFINES;DEPENDS;PUBLIC_DEPENDS;INCLUDES;PUBLIC_INCLUDES;SOURCES;"
        ${ARGN}
        )

    if (${_arg_UNPARSED_ARGUMENTS})
        message(FATAL_ERROR "add_qbs_test had unparsed arguments")
    endif()

    set(target_name "tst_${test_name}")

    string(TOUPPER ${test_name} suite_name) # cmake is beatiful, here we have <input, output>
    string(REPLACE - _ suite_name ${suite_name}) # and here we have <output, input>

    add_executable(${target_name} ${_arg_SOURCES})
    target_compile_definitions(${target_name} PRIVATE
        "QBS_TEST_SUITE_NAME=\"${suite_name}\""
        "SRCDIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        )
    target_compile_definitions(
        ${target_name} PRIVATE ${_arg_DEFINES} ${DEFAULT_DEFINES} PUBLIC ${_arg_PUBLIC_DEFINES})
    target_include_directories(
        ${target_name}
        PRIVATE ${_arg_INCLUDES} "${CMAKE_CURRENT_SOURCE_DIR}/../../../src"
        PUBLIC ${_arg_PUBLIC_INCLUDES}
        )
    target_link_libraries(
        ${target_name}
        PRIVATE ${_arg_DEPENDS} qbscore qbsconsolelogger Qt${QT_VERSION_MAJOR}::Test
        PUBLIC ${_arg_PUBLIC_DEPENDS}
        )

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_APP_INSTALL_DIR}
        BUILD_RPATH "${QBS_APP_RPATH}"
        INSTALL_RPATH "${QBS_APP_RPATH}"
        MACOSX_RPATH ${QBS_MACOSX_RPATH}
        )
    install(TARGETS ${target_name} RUNTIME DESTINATION ${QBS_APP_INSTALL_DIR})
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()
