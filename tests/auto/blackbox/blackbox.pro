TEMPLATE = app
TARGET = tst_blackbox
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core script testlib

include(../../../src/lib/use.pri)

HEADERS += \
    tst_blackbox.h

SOURCES += \
    tst_blackbox.cpp
