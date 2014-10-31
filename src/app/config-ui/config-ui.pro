include(../app.pri)

CONFIG -= console
QT += gui widgets

TARGET = qbs-config-ui

HEADERS += \
    ../shared/qbssettings.h \
    commandlineparser.h \
    mainwindow.h \
    settingsmodel.h

SOURCES += \
    ../shared/qbssettings.cpp \
    commandlineparser.cpp \
    main.cpp \
    mainwindow.cpp \
    settingsmodel.cpp

OTHER_FILES += \
    Info.plist

mac: QMAKE_LFLAGS += -sectcreate __TEXT __info_plist $$shell_quote($$PWD/Info.plist)

FORMS += mainwindow.ui
