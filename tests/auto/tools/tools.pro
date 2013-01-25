TEMPLATE = app
TARGET = tst_tools
DESTDIR = ./
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += depend_includepath testcase

include(../../../src/lib/use.pri)
include(../../../qbs_version.pri)
include(../../../src/app/qbs/parser/parser.pri)

SOURCES += tst_tools.cpp ../../../src/app/qbs/qbstool.cpp
