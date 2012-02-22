QT = core script
TEMPLATE = app
TARGET = qbs
DESTDIR = ../../../bin

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += main.cpp \
    ctrlchandler.cpp \
    application.cpp
HEADERS += \
    ctrlchandler.h \
    application.h \
    status.h

include(../../lib/use.pri)
