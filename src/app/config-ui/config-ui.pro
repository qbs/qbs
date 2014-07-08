include(../app.pri)

CONFIG -= console
QT += gui
greaterThan(QT_MAJOR_VERSION, 4):QT += widgets

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

FORMS += mainwindow.ui
