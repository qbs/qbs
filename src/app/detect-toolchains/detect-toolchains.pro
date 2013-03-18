include(../app.pri)

TARGET = qbs-detect-toolchains

HEADERS += probe.h msvcprobe.h ../shared/qbssettings.h osxprobe.h
SOURCES += main.cpp probe.cpp msvcprobe.cpp osxprobe.cpp
