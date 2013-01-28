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

include(../../lib/use.pri)

HEADERS += \
    configcommand.h \
    configcommandexecutor.h \
    configcommandlineparser.h \
    ../shared/qbssettings.h
