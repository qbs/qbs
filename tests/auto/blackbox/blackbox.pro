TEMPLATE = app
TARGET = tst_blackbox
DESTDIR = ./
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core script testlib
CONFIG += depend_includepath testcase

include(../../../src/lib/use.pri)

HEADERS += \
    tst_blackbox.h

SOURCES += \
    tst_blackbox.cpp
