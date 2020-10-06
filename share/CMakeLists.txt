if(QBS_RESOURCES_INSTALL_DIR)
    set(QBS_SHARE_DIR "${QBS_RESOURCES_INSTALL_DIR}/share")
else()
    set(QBS_SHARE_DIR "share")
endif()

if(WIN32)
    install(
        FILES ../bin/ibmsvc.xml ../bin/ibqbs.bat
        DESTINATION "${QBS_APP_INSTALL_DIR}"
        )
endif()
install(
    PROGRAMS ../src/3rdparty/python/bin/dmgbuild
    DESTINATION "${QBS_LIBEXEC_PATH}"
    )
add_custom_target(copy-runtime-files-dmgbuild ALL
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdparty/python/bin/dmgbuild
        ${CMAKE_BINARY_DIR}/${QBS_LIBEXEC_PATH}
    )
install(
    # trailing slash avoid copying the 'site-packages' dir and only copies its content
    DIRECTORY ../src/3rdparty/python/lib/python2.7/site-packages/
    DESTINATION "${QBS_SHARE_DIR}/qbs/python"
    FILES_MATCHING PATTERN "*.py"
    )
install(
    DIRECTORY qbs
    DESTINATION "${QBS_SHARE_DIR}")
add_custom_target(copy-runtime-files-qbs ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/qbs
        ${CMAKE_BINARY_DIR}/share/qbs
    )
install(
    DIRECTORY ../examples
    DESTINATION "${QBS_SHARE_DIR}/qbs"
    )

if(WIN32)
    set(UPDATE_PATH_COMMAND set "PATH=${QT_QMAKE_EXECUTABLE}/..\;%PATH%")
else()
    set(UPDATE_PATH_COMMAND "")
endif()

add_custom_target(
    BuildQbsResources ALL
    COMMAND ${UPDATE_PATH_COMMAND}
    COMMAND ${CMAKE_BINARY_DIR}/${QBS_APP_INSTALL_DIR}/qbs
        build
        --settings-dir ${CMAKE_BINARY_DIR}/settings
        -f ${CMAKE_SOURCE_DIR}/qbs.qbs
        -d ${CMAKE_BINARY_DIR}/
        -p "qbs resources"
        qbs.installPrefix:undefined
        project.withCode:false
        project.withDocumentation:false
        profile:none
    DEPENDS qbs
    )

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/default/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_SHARE_DIR}/qbs"
    )

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/default/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_SHARE_DIR}/qbs"
    )
