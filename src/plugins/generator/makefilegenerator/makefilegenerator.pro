include(makefilegenerator.pri)
include(../../plugins.pri)

QT = core

HEADERS += \
    $$PWD/makefilegenerator.h

SOURCES += \
    $$PWD/makefilegenerator.cpp \
    $$PWD/makefilegeneratorplugin.cpp
