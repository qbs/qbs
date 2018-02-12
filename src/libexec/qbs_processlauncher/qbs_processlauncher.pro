include(../libexec.pri)

TARGET = qbs_processlauncher
CONFIG += console c++14
CONFIG -= app_bundle
QT = core network

TOOLS_DIR = $$PWD/../../lib/corelib/tools

INCLUDEPATH += $$TOOLS_DIR

HEADERS += \
    launcherlogging.h \
    launchersockethandler.h \
    $$TOOLS_DIR/launcherpackets.h

SOURCES += \
    launcherlogging.cpp \
    launchersockethandler.cpp \
    processlauncher-main.cpp \
    $$TOOLS_DIR/launcherpackets.cpp
