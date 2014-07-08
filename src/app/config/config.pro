include(../app.pri)

TARGET = qbs-config

SOURCES += \
    ../shared/qbssettings.cpp \
    configcommandexecutor.cpp \
    configcommandlineparser.cpp \
    configmain.cpp

HEADERS += \
    ../shared/qbssettings.h \
    configcommand.h \
    configcommandexecutor.h \
    configcommandlineparser.h
