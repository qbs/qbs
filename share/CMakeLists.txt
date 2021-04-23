if(WIN32)
    install(
        FILES ../bin/ibmsvc.xml ../bin/ibqbs.bat
        DESTINATION "${QBS_APP_INSTALL_DIR}"
        )
endif()
install(
    PROGRAMS ../src/3rdparty/python/bin/dmgbuild
    DESTINATION "${QBS_LIBEXEC_INSTALL_DIR}"
    )
add_custom_target(copy-runtime-files-dmgbuild ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdparty/python/bin/dmgbuild
        ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_LIBEXEC_INSTALL_DIR}
    )
install(
    # trailing slash avoid copying the 'site-packages' dir and only copies its content
    DIRECTORY ../src/3rdparty/python/lib/python2.7/site-packages/
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs/python"
    FILES_MATCHING PATTERN "*.py"
    )
install(
    DIRECTORY qbs
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}")
add_custom_target(copy-runtime-files-qbs ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/qbs
        ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_RESOURCES_INSTALL_DIR}/qbs
    )
install(
    DIRECTORY ../examples
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )

get_update_path_command(UPDATE_PATH_COMMAND)
get_target_property(_QBS_OUTPUT_DIR qbs RUNTIME_OUTPUT_DIRECTORY)
add_custom_target(
    BuildQbsResources ALL
    COMMAND ${CMAKE_COMMAND} -E env "${UPDATE_PATH_COMMAND}" ${_QBS_OUTPUT_DIR}/qbs
        resolve
        --settings-dir ${PROJECT_BINARY_DIR}/settings
        -f ${PROJECT_SOURCE_DIR}/qbs.qbs
        -d ${PROJECT_BINARY_DIR}/
        config:resources-build
        qbs.installPrefix:undefined
        project.withCode:false
        project.withDocumentation:false
        profile:none
    COMMAND ${CMAKE_COMMAND} -E env "${UPDATE_PATH_COMMAND}" ${_QBS_OUTPUT_DIR}/qbs
        build
        --settings-dir ${PROJECT_BINARY_DIR}/settings
        -f ${PROJECT_SOURCE_DIR}/qbs.qbs
        -d ${PROJECT_BINARY_DIR}/
        config:resources-build
        -p "qbs resources"
    DEPENDS qbs copy-runtime-files-qbs qbs_processlauncher
    )

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/resources-build/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/resources-build/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )
