include(../app.pri)

TARGET = qbs-detect-toolchains

HEADERS += probe.h msvcprobe.h ../shared/qbssettings.h
SOURCES += main.cpp probe.cpp msvcprobe.cpp
