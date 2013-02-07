TEMPLATE = app
TARGET = tst_buildgraph
DESTDIR = ../../../bin
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += depend_includepath testcase
CONFIG   += console
CONFIG   -= app_bundle

SOURCES += \
    tst_buildgraph.cpp

include(../../../src/lib/use.pri)
include(../../../src/app/shared/logging/logging.pri)
