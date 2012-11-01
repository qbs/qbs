QT = core script
TEMPLATE = app
TARGET = qbs
DESTDIR = ../../../bin

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += main.cpp \
    ctrlchandler.cpp \
    application.cpp \
    showproperties.cpp \
    status.cpp \
    ../shared/commandlineparser.cpp \
    consoleprogressobserver.cpp

HEADERS += \
    ctrlchandler.h \
    application.h \
    showproperties.h \
    status.h \
    ../shared/commandlineparser.h \
    consoleprogressobserver.h

include(../../lib/use.pri)
include(../../../qbs_version.pri)
