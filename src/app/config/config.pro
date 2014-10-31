include(../app.pri)

TARGET = qbs-config

SOURCES += \
    configcommandexecutor.cpp \
    configcommandlineparser.cpp \
    configmain.cpp

HEADERS += \
    configcommand.h \
    configcommandexecutor.h \
    configcommandlineparser.h
