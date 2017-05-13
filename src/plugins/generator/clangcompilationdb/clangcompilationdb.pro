include(../../plugins.pri)

TARGET = clangcompilationdbgenerator

QT = core

HEADERS += \
    $$PWD/clangcompilationdbgenerator.h

SOURCES += \
    $$PWD/clangcompilationdbgenerator.cpp \
    $$PWD/clangcompilationdbgeneratorplugin.cpp
