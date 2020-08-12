set(SOURCES
    commandlineparser.cpp
    commandlineparser.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    )

# TODO: support Info.plist
if(APPLE)
    set(MACOS_SOURCES fgapp.mm)
    set(MACOS_FRAMEWORKS "-framework ApplicationServices" "-framework Cocoa")
endif()

add_qbs_app(qbs-config-ui
    DEPENDS corelib logginglib Qt5::Widgets ${MACOS_FRAMEWORKS}
    SOURCES ${SOURCES} ${MACOS_SOURCES}
    )
