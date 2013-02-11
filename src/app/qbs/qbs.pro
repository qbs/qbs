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
    consoleprogressobserver.cpp \
    commandlinefrontend.cpp \
    qbstool.cpp

HEADERS += \
    ctrlchandler.h \
    application.h \
    showproperties.h \
    status.h \
    consoleprogressobserver.h \
    commandlinefrontend.h \
    qbstool.h \
    ../shared/qbssettings.h

include(../../lib/use.pri)
include(parser/parser.pri)
include(../shared/logging/logging.pri)
