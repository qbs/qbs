INCLUDEPATH += $$PWD $$PWD/..  $$PWD/../..  $$PWD/../../..

HEADERS += \
    $$PWD/codelocation.h \
    $$PWD/error.h \
    $$PWD/fileinfo.h \
    $$PWD/filetime.h \
    $$PWD/logger.h \
    $$PWD/logsink.h \
    $$PWD/options.h \
    $$PWD/persistence.h \
    $$PWD/platform.h \
    $$PWD/scannerpluginmanager.h \
    $$PWD/scripttools.h \
    $$PWD/settings.h \
    $$PWD/coloredoutput.h \
    $$PWD/fakeconcurrent.h \
    $$PWD/platformglobals.h

SOURCES += \
    $$PWD/error.cpp \
    $$PWD/fileinfo.cpp \
    $$PWD/logger.cpp \
    $$PWD/logsink.cpp \
    $$PWD/options.cpp \
    $$PWD/persistence.cpp \
    $$PWD/platform.cpp \
    $$PWD/scannerpluginmanager.cpp \
    $$PWD/scripttools.cpp \
    $$PWD/settings.cpp \
    $$PWD/coloredoutput.cpp

win32 {
    SOURCES += $$PWD/filetime_win.cpp
}

unix {
    SOURCES += $$PWD/filetime_unix.cpp
}
