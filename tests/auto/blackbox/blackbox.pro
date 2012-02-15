TEMPLATE = app
TARGET = testBlackbox
DEPENDPATH += .
DESTDIR = ../../../bin/
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core script testlib

include(../../../src/lib/use.pri)

SOURCES += \
    tst_blackbox.cpp

