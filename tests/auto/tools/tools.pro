TEMPLATE = app
TARGET = tst_tools
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib

include(../../../src/lib/use.pri)

SOURCES += \
    tst_tools.cpp
