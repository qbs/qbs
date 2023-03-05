# Options:
option(QBS_INSTALL_HTML_DOCS "Whether to install Qbs HTML Documentation" OFF)
option(QBS_INSTALL_QCH_DOCS "Whether to install Qbs QCH Documentation" OFF)
option(QBS_INSTALL_MAN_PAGE "Whether to install Qbs man page" OFF)

# Get information on directories from qmake
# as this is not yet exported by cmake.
# Used for QT_INSTALL_DOCS
function(qt_query_qmake)
    if (NOT TARGET Qt${QT_VERSION_MAJOR}::qmake)
        message(FATAL_ERROR "Qmake was not found. Add find_package(Qt5 COMPONENTS Core) to CMake to enable.")
    endif()
    # dummy check for if we already queried qmake
    if (QT_INSTALL_BINS)
        return()
    endif()

    get_target_property(_qmake_binary Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    execute_process(COMMAND "${_qmake_binary}" "-query"
        TIMEOUT 10
        RESULT_VARIABLE _qmake_result
        OUTPUT_VARIABLE _qmake_stdout
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if (NOT "${_qmake_result}" STREQUAL "0")
        message(FATAL_ERROR "Qmake did not execute successfully: ${_qmake_result}.")
    endif()

    # split into lines:
    string(REPLACE "\n" ";" _lines "${_qmake_stdout}")

    foreach(_line ${_lines})
        # split line into key/value pairs
        string(REPLACE ":" ";" _parts "${_line}")
        list(GET _parts 0 _key)
        list(REMOVE_AT _parts 0)
        string(REPLACE ";" ":" _value "${_parts}")

        set("${_key}" "${_value}" CACHE PATH "qmake import of ${_key}" FORCE)
    endforeach()
endfunction()

function(_qbs_setup_doc_targets)
    # Set up important targets:
    if (NOT TARGET qbs_html_docs)
        add_custom_target(qbs_html_docs COMMENT "Build HTML documentation")
    endif()
    if (NOT TARGET qbs_qch_docs)
        add_custom_target(qbs_qch_docs COMMENT "Build QCH documentation")
    endif()
    if (NOT TARGET BuildQbsDocumentation)
        add_custom_target(
            BuildQbsDocumentation ALL COMMENT "Build Qbs documentation")
        add_dependencies(BuildQbsDocumentation qbs_html_docs qbs_qch_docs)
    endif()
    if (NOT TARGET qbs_docs)
        add_custom_target(
            qbs_docs ALL COMMENT "Build Qbs documentation")
        add_dependencies(qbs_docs BuildQbsDocumentation)
    endif()
endfunction()

function(_find_python_module module)
    string(TOUPPER ${module} module_upper)
    if (NOT PY_${module_upper})
        if (ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
            set(${module}_FIND_REQUIRED TRUE)
        endif()
        # A module's location is usually a directory, but for binary modules
        # it's a .so file.
        execute_process(COMMAND "${Python3_EXECUTABLE}" "-c"
            "import re, ${module}; print(re.compile('/__init__.py.*').sub('',${module}.__file__))"
            RESULT_VARIABLE _${module}_status
            OUTPUT_VARIABLE _${module}_location
            ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (NOT _${module}_status)
            set(PY_${module_upper} ${_${module}_location} CACHE STRING
                "Location of Python module ${module}")
        endif()
    endif()
    find_package_handle_standard_args(PY_${module} DEFAULT_MSG PY_${module_upper})
endfunction()

function(_qbs_setup_qdoc_targets _qdocconf_file _retval)
    cmake_parse_arguments(_arg "" "HTML_DIR;INSTALL_DIR;POSTFIX"
        "INDEXES;INCLUDE_DIRECTORIES;FRAMEWORK_PATHS;ENVIRONMENT_EXPORTS;SOURCES" ${ARGN})

    foreach(_index ${_arg_INDEXES})
        list(APPEND _qdoc_index_args "-indexdir;${_index}")
    endforeach()

    set(_env "")
    foreach(_export ${_arg_ENVIRONMENT_EXPORTS})
        if (NOT DEFINED "${_export}")
            message(FATAL_ERROR "${_export} is not known when trying to export it to qdoc.")
        endif()
        list(APPEND _env "${_export}=${${_export}}")
    endforeach()

    get_target_property(_qdoc Qt${QT_VERSION_MAJOR}::qdoc IMPORTED_LOCATION)

    set(_full_qdoc_command "${_qdoc}")
    if (_env)
        set(_full_qdoc_command "${CMAKE_COMMAND}" "-E" "env" ${_env} "${_qdoc}")
    endif()

    if (_arg_HTML_DIR STREQUAL "")
        set(_arg_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
    endif()

    get_filename_component(_target "${_qdocconf_file}" NAME_WE)

    set(_html_outputdir "${_arg_HTML_DIR}")
    file(MAKE_DIRECTORY "${_html_outputdir}")

    set(_qdoc_include_args "")
    if (_arg_INCLUDE_DIRECTORIES OR _arg_FRAMEWORK_PATHS)
        # pass include directories to qdoc via hidden @ option, since we need to generate a file
        # to be able to resolve the generators inside the include paths
        set(_qdoc_includes "${CMAKE_CURRENT_BINARY_DIR}/cmake/qdoc_${_target}.inc")
        set(_qdoc_include_args "@${_qdoc_includes}")
        set(_includes "")
        if (_arg_INCLUDE_DIRECTORIES)
            set(_includes "-I$<JOIN:${_arg_INCLUDE_DIRECTORIES},\n-I>\n")
        endif()
        set(_frameworks "")
        if (_arg_FRAMEWORK_PATHS)
            set(_frameworks "-F$<JOIN:${_arg_FRAMEWORK_PATHS},\n-F>\n")
        endif()
        file(GENERATE
            OUTPUT "${_qdoc_includes}"
            CONTENT "${_includes}${_frameworks}"
            )
    endif()

    set(_html_artifact "${_html_outputdir}/index.html")
    add_custom_command(
        OUTPUT "${_html_artifact}"
        COMMAND cmake -E remove_directory "${_html_outputdir}"
        COMMAND ${_full_qdoc_command} -outputdir "${_html_outputdir}" "${_qdocconf_file}"
            ${_qdoc_index_args} ${_qdoc_include_args}
        DEPENDS "${_qdocconf_file}" ${_arg_SOURCES}
        COMMENT "Build HTML documentation from ${_qdocconf_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
        )

    if (NOT Python3_Interpreter_FOUND)
        message(WARNING "Cannot find python3 binary. Qbs documentation will not be built.")
        return()
    endif()
    _find_python_module(lxml)
    _find_python_module(bs4)
    if (NOT PY_LXML OR NOT PY_BS4)
        message(WARNING "Cannot import lxml and bs4 python modules. Qbs documentation will not be built.")
        return()
    endif()

    set(_fixed_html_artifact "${CMAKE_CURRENT_BINARY_DIR}/qbsdoc.dummy")
    set(_fix_qml_imports_script ${Qbs_SOURCE_DIR}/doc/fix-qmlimports.py)
    add_custom_command(
        OUTPUT "${_fixed_html_artifact}"
        COMMAND ${Python3_EXECUTABLE} "${_fix_qml_imports_script}" ${_html_outputdir}
        COMMAND cmake -E touch ${_fixed_html_artifact}
        DEPENDS "${_html_artifact}" "${_fix_qml_imports_script}"
        COMMENT "Fixing bogus QML import statements"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
        )

    set(_html_target "qbs_html_docs_${_target}")
    add_custom_target(${_html_target} DEPENDS "${_fixed_html_artifact}")
    add_dependencies(qbs_html_docs "${_html_target}")

    # artifacts might be required for QCH-only installation, so we build them
    # always, and skip HTML docs installation here
    if (QBS_INSTALL_HTML_DOCS)
        install(DIRECTORY "${_html_outputdir}" DESTINATION "${_arg_INSTALL_DIR}"
            COMPONENT qbs_docs)
    endif()

    set("${_retval}" "${_html_outputdir}" PARENT_SCOPE)
endfunction()

function(_qbs_setup_qhelpgenerator_targets _qdocconf_file _html_outputdir)
    cmake_parse_arguments(_arg "" "QCH_DIR;INSTALL_DIR" "" ${ARGN})
    if (_arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "qdoc_build_qdocconf_file has unknown arguments: ${_arg_UNPARSED_ARGUMENTS}.")
    endif()

    if (NOT _arg_QCH_DIR)
        set(_arg_QCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
    endif()

    if (NOT TARGET Qt${QT_VERSION_MAJOR}::qhelpgenerator)
        message(WARNING "qhelpgenerator missing: No QCH documentation targets were generated. Add find_package(Qt5 COMPONENTS Help) to CMake to enable.")
        return()
    endif()

    get_filename_component(_target "${_qdocconf_file}" NAME_WE)

    set(_html_target "qbs_html_docs_${_target}")
    if (NOT TARGET ${_html_target})
        return()
    endif()

    set(_qch_outputdir "${_arg_QCH_DIR}")
    file(MAKE_DIRECTORY "${_qch_outputdir}")

    set(_fixed_html_artifact "${CMAKE_CURRENT_BINARY_DIR}/qbsdoc.dummy")
    set(_qhp_artifact "${_html_outputdir}/${_target}.qhp")
    set(_qch_artifact "${_qch_outputdir}/${_target}.qch")
    add_custom_command(
        OUTPUT "${_qch_artifact}"
        COMMAND Qt${QT_VERSION_MAJOR}::qhelpgenerator "${_qhp_artifact}" -o "${_qch_artifact}"
        DEPENDS "${_fixed_html_artifact}"
        COMMENT "Build QCH documentation from ${_qdocconf_file}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
        )

    set(_qch_target "qbs_qch_docs_${_target}")
    add_custom_target("${_qch_target}" DEPENDS "${_qch_artifact}")
    add_dependencies(qbs_qch_docs "${_qch_target}")

    install(FILES "${_qch_outputdir}/${_target}.qch" DESTINATION "${_arg_INSTALL_DIR}"
        COMPONENT qbs_docs)
endfunction()

# Helper functions:
function(_qbs_qdoc_build_qdocconf_file _qdocconf_file)
    _qbs_setup_doc_targets()

    if (NOT TARGET Qt${QT_VERSION_MAJOR}::qdoc)
        message(WARNING "No qdoc binary found: No documentation targets were generated")
        return()
    endif()

    cmake_parse_arguments(_arg "QCH" "HTML_DIR;QCH_DIR;INSTALL_DIR;POSTFIX"
        "INDEXES;INCLUDE_DIRECTORIES;FRAMEWORK_PATHS;ENVIRONMENT_EXPORTS;SOURCES" ${ARGN})
    if (_arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "qdoc_build_qdocconf_file has unknown arguments: ${_arg_UNPARSED_ARGUMENTS}.")
    endif()

    if (NOT _arg_INSTALL_DIR)
        message(FATAL_ERROR "No INSTALL_DIR set when calling qdoc_build_qdocconf_file")
    endif()

    _qbs_setup_qdoc_targets("${_qdocconf_file}" _html_outputdir
        HTML_DIR "${_arg_HTML_DIR}" INSTALL_DIR "${_arg_INSTALL_DIR}"
        INDEXES ${_arg_INDEXES} ENVIRONMENT_EXPORTS ${_arg_ENVIRONMENT_EXPORTS}
        POSTFIX "${_arg_POSTFIX}"
        INCLUDE_DIRECTORIES ${_arg_INCLUDE_DIRECTORIES}
        FRAMEWORK_PATHS ${_arg_FRAMEWORK_PATHS}
        SOURCES ${_arg_SOURCES}
        )

    if (_arg_QCH)
        _qbs_setup_qhelpgenerator_targets("${_qdocconf_file}" "${_html_outputdir}"
            QCH_DIR "${_arg_QCH_DIR}" INSTALL_DIR "${_arg_INSTALL_DIR}")
    endif()

endfunction()

function(add_qbs_documentation qdocconf_file)
    cmake_parse_arguments(_arg "" ""
        "INCLUDE_DIRECTORIES;FRAMEWORK_PATHS;SOURCES" ${ARGN})
    if (_arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_qbs_documentation has unknown arguments: ${_arg_UNPARSED_ARGUMENTS}.")
    endif()

    ### Skip docs setup if that is not needed!
    if (NOT QBS_INSTALL_HTML_DOCS AND NOT QBS_INSTALL_QCH_DOCS)
        return()
    endif()

    qt_query_qmake()
    set(SRCDIR "${Qbs_SOURCE_DIR}/doc")

    set(_qch_params)
    # if QBS_INSTALL_QCH_DOCS is No, qch generation will be skipped entirely
    if (QBS_INSTALL_QCH_DOCS)
        set(_qch_params QCH QCH_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(_qdoc_params HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/${QBS_DOC_HTML_DIR_NAME}")
    list(APPEND _qdoc_params INSTALL_DIR "${QBS_DOC_INSTALL_DIR}")

    # Set up environment for qdoc:
    string(REPLACE "." "" QBS_VERSION_TAG "${QBS_VERSION}")

    list(APPEND _qdoc_params ENVIRONMENT_EXPORTS
        SRCDIR QBS_VERSION QBS_VERSION_TAG
        QT_INSTALL_DOCS
        )

    _qbs_qdoc_build_qdocconf_file(${qdocconf_file} ${_qch_params} ${_qdoc_params}
        INCLUDE_DIRECTORIES ${_arg_INCLUDE_DIRECTORIES}
        FRAMEWORK_PATHS ${_arg_FRAMEWORK_PATHS}
        SOURCES ${_arg_SOURCES}
        )
endfunction()
