include(../app.pri)

TARGET = qbs-setup-toolchains

HEADERS += commandlineparser.h probe.h msvcprobe.h xcodeprobe.h ../shared/qbssettings.h
SOURCES += commandlineparser.cpp main.cpp probe.cpp msvcprobe.cpp xcodeprobe.cpp

mingw {
    RC_FILE = qbs-setup-toolchains.rc
}
