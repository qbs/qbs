INCLUDEPATH += $$PWD $$PWD/..  $$PWD/../..  $$PWD/../../..

HEADERS += \
    $$PWD/codelocation.h \
    $$PWD/error.h \
    $$PWD/fileinfo.h \
    $$PWD/filetime.h \
    $$PWD/persistence.h \
    $$PWD/platform.h \
    $$PWD/scannerpluginmanager.h \
    $$PWD/scripttools.h \
    $$PWD/settings.h \
    $$PWD/progressobserver.h \
    $$PWD/hostosinfo.h \
    $$PWD/buildoptions.h \
    $$PWD/runenvironment.h \
    $$PWD/processoutput.h

SOURCES += \
    $$PWD/error.cpp \
    $$PWD/fileinfo.cpp \
    $$PWD/persistence.cpp \
    $$PWD/platform.cpp \
    $$PWD/scannerpluginmanager.cpp \
    $$PWD/scripttools.cpp \
    $$PWD/settings.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/processoutput.cpp

win32 {
    SOURCES += $$PWD/filetime_win.cpp
}

unix {
    SOURCES += $$PWD/filetime_unix.cpp
}
