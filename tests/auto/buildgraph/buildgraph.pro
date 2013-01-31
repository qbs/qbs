TEMPLATE = app
TARGET = tst_buildgraph
DESTDIR = ./
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += depend_includepath testcase

HEADERS += \
    tst_buildgraph.h

SOURCES += \
    tst_buildgraph.cpp

include(../../../src/lib/use.pri)
include(../../../src/app/shared/logging/logging.pri)
