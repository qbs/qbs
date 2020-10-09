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

if(WIN32)
    get_target_property(_QTCORE_LIBRARY Qt5::Core IMPORTED_LOCATION_RELEASE)
    if(NOT _QTCORE_LIBRARY)
        get_target_property(_QTCORE_LIBRARY Qt5::Core IMPORTED_LOCATION_DEBUG)
    endif()
    get_filename_component(_QT_LIBRARY_PATH "${_QTCORE_LIBRARY}" DIRECTORY)
    get_target_property(_QBS_LIBRARY_PATH qbscore LIBRARY_OUTPUT_DIRECTORY)
    set(UPDATE_PATH_COMMAND set "PATH=${_QT_LIBRARY_PATH}\;${_QBS_LIBRARY_PATH}\;%PATH%")
else()
    set(UPDATE_PATH_COMMAND "")
endif()

get_target_property(_QBS_OUTPUT_DIR qbs RUNTIME_OUTPUT_DIRECTORY)
add_custom_target(
    BuildQbsResources ALL
    COMMAND ${UPDATE_PATH_COMMAND}
    COMMAND ${_QBS_OUTPUT_DIR}/qbs
        build
        --settings-dir ${PROJECT_BINARY_DIR}/settings
        -f ${PROJECT_SOURCE_DIR}/qbs.qbs
        -d ${PROJECT_BINARY_DIR}/
        -p "qbs resources"
        qbs.installPrefix:undefined
        project.withCode:false
        project.withDocumentation:false
        profile:none
    DEPENDS qbs copy-runtime-files-qbs
    )

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/default/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/default/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )
