include(../../plugins.pri)

TARGET = makefilegenerator

QT = core

HEADERS += \
    $$PWD/makefilegenerator.h

SOURCES += \
    $$PWD/makefilegenerator.cpp \
    $$PWD/makefilegeneratorplugin.cpp
