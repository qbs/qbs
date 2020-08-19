set(QBS_APP_INSTALL_DIR "bin")
set(QBS_LIBDIR_NAME "lib")

if(APPLE)
    set(QBS_LIB_RPATH "@loader_path")
    set(QBS_LIBEXEC_RPATH "@loader_path/../../${QBS_LIBDIR_NAME}")
    set(QBS_APP_RPATH "@loader_path/../${QBS_LIBDIR_NAME}")
    set(QBS_PLUGINS_RPATH "@loader_path/../../../${QBS_LIBDIR_NAME}")
elseif(WIN32)
    set(QBS_LIB_RPATH "")
    set(QBS_LIBEXEC_RPATH "")
    set(QBS_APP_RPATH "")
    set(QBS_PLUGINS_RPATH "")
else()
    set(QBS_LIB_RPATH "\$ORIGIN")
    set(QBS_LIBEXEC_RPATH "\$ORIGIN/../../${QBS_LIBDIR_NAME}")
    set(QBS_APP_RPATH "\$ORIGIN/../${QBS_LIBDIR_NAME}")
    set(QBS_PLUGINS_RPATH "\$ORIGIN/../../../${QBS_LIBDIR_NAME}")
endif()

if(WIN32)
    set(QBS_LIB_INSTALL_DIR ${QBS_APP_INSTALL_DIR})
    set(QBS_LIBEXEC_PATH ${QBS_APP_INSTALL_DIR})
else()
    set(QBS_LIB_INSTALL_DIR ${QBS_LIBDIR_NAME})
    set(QBS_LIBEXEC_PATH "libexec/qbs")
endif()

if(WITH_UNIT_TESTS)
    set(QBS_UNIT_TESTS_DEFINES "QBS_ENABLE_UNIT_TESTS")
else()
    set(QBS_UNIT_TESTS_DEFINES "")
endif()

if(WITH_PROJECT_FILE_UPDATES)
    set(QBS_PROJECT_FILE_UPDATES_DEFINES "QBS_ENABLE_PROJECT_FILE_UPDATES")
else()
    set(QBS_PROJECT_FILE_UPDATES_DEFINES "")
endif()

set(QBS_PLUGINS_INSTALL_DIR "${QBS_LIBDIR_NAME}/qbs/plugins")
set(QBS_RELATIVE_LIBEXEC_PATH "../${QBS_LIBEXEC_PATH}")
set(QBS_RELATIVE_SEARCH_PATH "..")
set(QBS_RELATIVE_PLUGINS_PATH "../${QBS_LIBDIR_NAME}")
set(QBS_RESOURCES_INSTALL_DIR "")
set(QBS_HEADERS_INSTALL_DIR "include/qbs")

set(DEFAULT_DEFINES "")
if(WIN32)
  list(APPEND DEFAULT_DEFINES UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
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
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${_DESTINATION}
        BUILD_RPATH "${QBS_APP_RPATH}"
        INSTALL_RPATH "${QBS_APP_RPATH}"
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

    string(REGEX REPLACE "\\.[0..9]+$" "" _SOVERSION ${QBS_VERSION})

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
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_LIB_INSTALL_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_LIB_INSTALL_DIR}
        BUILD_RPATH "${QBS_LIB_RPATH}"
        INSTALL_RPATH "${QBS_LIB_RPATH}"
        SOVERSION ${_SOVERSION}
        VERSION ${QBS_VERSION}
        )
    if (NOT _arg_STATIC)
        install(TARGETS ${target_name} LIBRARY DESTINATION ${QBS_LIB_INSTALL_DIR})
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
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_PLUGINS_INSTALL_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_PLUGINS_INSTALL_DIR}
        BUILD_RPATH "${QBS_PLUGINS_RPATH}"
        INSTALL_RPATH "${QBS_PLUGINS_RPATH}"
        )
    install(TARGETS ${target_name} LIBRARY DESTINATION ${QBS_PLUGINS_INSTALL_DIR})
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
        ${target_name} PRIVATE ${_arg_INCLUDES} PUBLIC ${_arg_PUBLIC_INCLUDES})
    target_link_libraries(
        ${target_name}
        PRIVATE ${_arg_DEPENDS} qbscore qbslogging Qt5::Test
        PUBLIC ${_arg_PUBLIC_DEPENDS}
        )

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${QBS_APP_INSTALL_DIR}
        BUILD_RPATH "${QBS_APP_RPATH}"
        INSTALL_RPATH "${QBS_APP_RPATH}"
        )
    install(TARGETS ${target_name} RUNTIME DESTINATION ${QBS_APP_INSTALL_DIR})
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()
