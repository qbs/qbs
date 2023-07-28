if(WIN32)
    install(
        FILES ../bin/ibmsvc.xml ../bin/ibqbs.bat
        DESTINATION "${QBS_APP_INSTALL_DIR}"
        )
endif()

# below we copy some files that are required to run qbs from the build directory
# emulating the same layout we will have after the installation

# copy & install python packages
set(_SITE_PACKAGES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdparty/python/lib/python3.9/site-packages/)
file(GLOB_RECURSE _SITE_PACKAGES_RELATIVE RELATIVE "${_SITE_PACKAGES_DIR}" "${_SITE_PACKAGES_DIR}/*.py")

set(_SITE_PACKAGES_SRC ${_SITE_PACKAGES_RELATIVE})
list_transform_prepend(_SITE_PACKAGES_SRC "${_SITE_PACKAGES_DIR}/")

set(_SITE_PACKAGES_DST ${_SITE_PACKAGES_RELATIVE})
list_transform_prepend(_SITE_PACKAGES_DST "${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_RESOURCES_INSTALL_DIR}/qbs/python/")

add_custom_command(
    OUTPUT ${_SITE_PACKAGES_DST}
    DEPENDS ${_SITE_PACKAGES_SRC}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${_SITE_PACKAGES_DIR}"
        ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_RESOURCES_INSTALL_DIR}/qbs/python
    COMMENT "Copying Python packages"
    )
install(
    # trailing slash avoid copying the 'site-packages' dir and only copies its content
    DIRECTORY "${_SITE_PACKAGES_DIR}"
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs/python"
    FILES_MATCHING PATTERN "*.py"
    )

# copy & install the "share/qbs" dir
file(GLOB_RECURSE _QBS_RESOURCES_RELATIVE RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/qbs/*")

set(_QBS_RESOURCES_SRC ${_QBS_RESOURCES_RELATIVE})
list_transform_prepend(_QBS_RESOURCES_SRC "${CMAKE_CURRENT_SOURCE_DIR}/")

set(_QBS_RESOURCES_DST ${_QBS_RESOURCES_RELATIVE})
list_transform_prepend(_QBS_RESOURCES_DST "${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_RESOURCES_INSTALL_DIR}/")

add_custom_command(
    OUTPUT ${_QBS_RESOURCES_DST}
    DEPENDS ${_QBS_RESOURCES_SRC}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/qbs
        ${CMAKE_BINARY_DIR}/${QBS_OUTPUT_PREFIX}${QBS_RESOURCES_INSTALL_DIR}/qbs
    COMMENT "Copying QBS resources"
    )
install(
    DIRECTORY qbs
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}")

# install examples dir
install(
    DIRECTORY ../examples
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )

set(_QMLTYPES_INSTALL_PATH "${PROJECT_BINARY_DIR}/resources-build/install-root/share/qbs/qml-type-descriptions/qbs.qmltypes")
get_update_path_command(UPDATE_PATH_COMMAND)
get_target_property(_QBS_OUTPUT_DIR qbs RUNTIME_OUTPUT_DIRECTORY)
add_custom_command(
    OUTPUT "${_QMLTYPES_INSTALL_PATH}"
    COMMAND ${CMAKE_COMMAND} -E env "${UPDATE_PATH_COMMAND}" ${_QBS_OUTPUT_DIR}/qbs
        resolve
        --settings-dir ${PROJECT_BINARY_DIR}/settings
        -f ${PROJECT_SOURCE_DIR}/qbs.qbs
        -d ${PROJECT_BINARY_DIR}/
        config:resources-build
        qbs.installPrefix:""
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
    DEPENDS qbs "${_QBS_RESOURCES_DST}" qbs_processlauncher
    )

add_custom_target(
    BuildQbsResources ALL
    DEPENDS
    ${_SITE_PACKAGES_DST}
    ${_QBS_RESOURCES_DST}
    "${_QMLTYPES_INSTALL_PATH}"
    )

install(
    DIRECTORY ${PROJECT_BINARY_DIR}/resources-build/install-root/share/qbs/qml-type-descriptions
    DESTINATION "${QBS_RESOURCES_INSTALL_DIR}/qbs"
    )
