QT       += core
QT       -= gui

TARGET = qbs-setup-madde-platforms
CONFIG   += console
CONFIG   -= app_bundle
DESTDIR = ../../../bin

TEMPLATE = app

SOURCES += main.cpp ../shared/specialplatformssetup.cpp
HEADERS += ../shared/specialplatformssetup.h

include(../../lib/use.pri)
