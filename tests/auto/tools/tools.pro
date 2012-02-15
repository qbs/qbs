TEMPLATE = app
TARGET = testTools
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib

include(../../../src/lib/use.pri)

SOURCES += \
    tst_tools.cpp
