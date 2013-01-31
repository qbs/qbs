QT       += core
QT       -= gui

TARGET = qbs-setup-madde-platforms
CONFIG   += console
CONFIG   -= app_bundle
DESTDIR = ../../../bin

TEMPLATE = app

SOURCES += main.cpp ../shared/specialplatformssetup.cpp
HEADERS += ../shared/specialplatformssetup.h ../shared/qbssettings.h

include(../../lib/use.pri)
include(../shared/logging/logging.pri)
