if (QBS_INSTALL_HTML_DOCS OR QBS_INSTALL_QCH_DOCS)
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
            project.withCode:false
            project.withDocumentation:true
            profile:none
            modules.qbsbuildconfig.installHtml:${_INSTALL_HTML_DOCS}
            modules.qbsbuildconfig.installQch:${_INSTALL_QCH_DOCS}
            moduleProviders.Qt.qmakeFilePaths:${QT_QMAKE_EXECUTABLE}
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
endif()
