include(../app.pri)

TARGET = qbs-setup-toolchains

HEADERS += \
    commandlineparser.h \
    msvcprobe.h \
    probe.h \
    xcodeprobe.h

SOURCES += \
    commandlineparser.cpp \
    main.cpp \
    msvcprobe.cpp \
    probe.cpp \
    xcodeprobe.cpp

mingw {
    RC_FILE = qbs-setup-toolchains.rc
}
