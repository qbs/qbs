TEMPLATE = app
TARGET = tst_language
DESTDIR = $$OUT_PWD
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent

include(../../../src/lib/use.pri)

HEADERS += \
    tst_language.h

SOURCES += \
    tst_language.cpp

OTHER_FILES += \
    $$PWD/testdata/*
