TEMPLATE = app
TARGET = tst_tools
DESTDIR = $$OUT_PWD
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core testlib
CONFIG += testcase

include(../../../src/lib/use.pri)
include(../../../qbs_version.pri)

HEADERS += \
    ../../../src/app/shared/commandlineparser.h
SOURCES += tst_tools.cpp \
    ../../../src/app/shared/commandlineparser.cpp
