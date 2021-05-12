set(SOURCES
    launcherlogging.cpp
    launcherlogging.h
    launchersockethandler.cpp
    launchersockethandler.h
    processlauncher-main.cpp
    )

set(PATH_TO_PROTOCOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/corelib/tools")
set(PROTOCOL_SOURCES
    launcherpackets.cpp
    launcherpackets.h
    )
list_transform_prepend(PROTOCOL_SOURCES ${PATH_TO_PROTOCOL_SOURCES}/)

add_qbs_app(qbs_processlauncher
    DESTINATION ${QBS_LIBEXEC_INSTALL_DIR}
    DEPENDS Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${PATH_TO_PROTOCOL_SOURCES}
    SOURCES ${SOURCES} ${PROTOCOL_SOURCES}
    )
set_target_properties(qbs_processlauncher PROPERTIES
    BUILD_RPATH "${QBS_LIBEXEC_RPATH}"
    INSTALL_RPATH "${QBS_LIBEXEC_RPATH}"
    )
