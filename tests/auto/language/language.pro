TEMPLATE = app
TARGET = tst_language
DESTDIR = ./
INCLUDEPATH += ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += depend_includepath testcase

include(../../../src/lib/use.pri)

HEADERS += \
    tst_language.h

SOURCES += \
    tst_language.cpp

OTHER_FILES += \
    $$PWD/testdata/* \
    testdata/outerInGroup.qbs
