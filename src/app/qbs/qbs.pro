include(../app.pri)
include(parser/parser.pri)

TARGET = qbs

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
