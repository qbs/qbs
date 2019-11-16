include(clangcompilationdb.pri)
include(../../plugins.pri)

QT = core

HEADERS += \
    $$PWD/clangcompilationdbgenerator.h

SOURCES += \
    $$PWD/clangcompilationdbgenerator.cpp \
    $$PWD/clangcompilationdbgeneratorplugin.cpp
