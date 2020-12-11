if (QBS_INSTALL_HTML_DOCS OR QBS_INSTALL_QCH_DOCS)

    find_package(Python3 COMPONENTS Interpreter)

    function(find_python_module module)
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

    if (Python3_Interpreter_FOUND)
        find_python_module(lxml)
        find_python_module(bs4)
        if (PY_LXML AND PY_BS4)

            get_target_property(_QBS_OUTPUT_DIR qbs RUNTIME_OUTPUT_DIRECTORY)

            if (QBS_INSTALL_HTML_DOCS)
                set(_INSTALL_HTML_DOCS true)
            else()
                set(_INSTALL_HTML_DOCS false)
            endif()

            if (QBS_INSTALL_QCH_DOCS)
                set(_INSTALL_QCH_DOCS true)
            else()
                set(_INSTALL_QCH_DOCS false)
            endif()

            get_target_property(_QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
            get_update_path_command(UPDATE_PATH_COMMAND)
            add_custom_target(
                BuildQbsDocumentation ALL
                COMMAND ${UPDATE_PATH_COMMAND}
                COMMAND ${_QBS_OUTPUT_DIR}/qbs
                resolve
                --settings-dir ${PROJECT_BINARY_DIR}/settings
                -f ${PROJECT_SOURCE_DIR}/qbs.qbs
                -d ${PROJECT_BINARY_DIR}/
                config:doc-build
                qbs.installPrefix:undefined
                qbs.buildVariant:release
                project.withCode:false
                project.withDocumentation:true
                profile:none
                "products.qbs documentation.pythonPath:${Python3_EXECUTABLE}"
                modules.qbsbuildconfig.installHtml:${_INSTALL_HTML_DOCS}
                modules.qbsbuildconfig.installQch:${_INSTALL_QCH_DOCS}
                moduleProviders.Qt.qmakeFilePaths:${_QT_QMAKE_EXECUTABLE}
                COMMAND ${_QBS_OUTPUT_DIR}/qbs
                build
                --settings-dir ${PROJECT_BINARY_DIR}/settings
                -f ${PROJECT_SOURCE_DIR}/qbs.qbs
                -d ${PROJECT_BINARY_DIR}/
                config:doc-build
                -p "qbs documentation"
                DEPENDS qbs copy-runtime-files-qbs qbs_processlauncher
                )
            install(
                DIRECTORY ${PROJECT_BINARY_DIR}/doc-build/install-root/share/doc/qbs/html/
                DESTINATION "${QBS_DOC_INSTALL_DIR}"
                )
        else()
            message(WARNING "Cannot import lxml and bs4 python modules. Qbs documentation will not be built.")
        endif()
    else()
        message(WARNING "Cannot find python3 binary. Qbs documentation will not be built.")
    endif()
endif()
