include(../app.pri)

CONFIG -= console
QT += gui widgets

TARGET = qbs-config-ui

HEADERS += \
    commandlineparser.h \
    mainwindow.h

SOURCES += \
    commandlineparser.cpp \
    main.cpp \
    mainwindow.cpp

OTHER_FILES += \
    Info.plist

osx {
    QMAKE_LFLAGS += -sectcreate __TEXT __info_plist $$shell_quote($$PWD/Info.plist)
    OBJECTIVE_SOURCES += fgapp.mm
    LIBS += -framework ApplicationServices -framework Cocoa
}

FORMS += mainwindow.ui
