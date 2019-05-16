include(../app.pri)

TARGET = qbs-setup-toolchains

HEADERS += \
    clangclprobe.h \
    commandlineparser.h \
    iarewprobe.h \
    msvcprobe.h \
    probe.h \
    sdccprobe.h \
    xcodeprobe.h \

SOURCES += \
    clangclprobe.cpp \
    commandlineparser.cpp \
    iarewprobe.cpp \
    main.cpp \
    msvcprobe.cpp \
    probe.cpp \
    sdccprobe.cpp \
    xcodeprobe.cpp \

mingw {
    RC_FILE = qbs-setup-toolchains.rc
}
