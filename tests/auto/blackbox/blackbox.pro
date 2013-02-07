TEMPLATE = app
TARGET = tst_blackbox
DESTDIR = ../../../bin
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core script testlib
CONFIG += depend_includepath testcase
CONFIG   += console
CONFIG   -= app_bundle

include(../../../src/lib/use.pri)

HEADERS += \
    tst_blackbox.h

SOURCES += \
    tst_blackbox.cpp
