TEMPLATE = app
TARGET = testDependencyFinder
DEPENDPATH += .
INCLUDEPATH += . ../../../src/lib/
DEFINES += SRCDIR=\\\"$$PWD/\\\"

QT = core script testlib

SOURCES += \
    tst_dependencyFinder.cpp
