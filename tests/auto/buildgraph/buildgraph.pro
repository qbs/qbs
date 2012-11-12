TEMPLATE = app
TARGET = tst_buildgraph
DESTDIR = ./
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += testcase

include(../../../src/lib/use.pri)

HEADERS += \
    tst_buildgraph.h

SOURCES += \
    tst_buildgraph.cpp
