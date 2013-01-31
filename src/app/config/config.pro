TARGET = qbs-config
TEMPLATE = app
QT = core

CONFIG += console
CONFIG -= app_bundle
DESTDIR = ../../../bin/

SOURCES = \
    configcommandexecutor.cpp \
    configcommandlineparser.cpp \
    configmain.cpp

HEADERS += \
    configcommand.h \
    configcommandexecutor.h \
    configcommandlineparser.h \
    ../shared/qbssettings.h

include(../../lib/use.pri)
include(../shared/logging/logging.pri)
