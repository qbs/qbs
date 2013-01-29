INCLUDEPATH += $$PWD/../.. # for plugins

HEADERS += \
    $$PWD/codelocation.h \
    $$PWD/error.h \
    $$PWD/fileinfo.h \
    $$PWD/filetime.h \
    $$PWD/persistence.h \
    $$PWD/scannerpluginmanager.h \
    $$PWD/scripttools.h \
    $$PWD/settings.h \
    $$PWD/preferences.h \
    $$PWD/profile.h \
    $$PWD/processresult.h \
    $$PWD/progressobserver.h \
    $$PWD/hostosinfo.h \
    $$PWD/buildoptions.h \
    $$PWD/installoptions.h \
    $$PWD/setupprojectparameters.h \
    $$PWD/persistentobject.h \
    $$PWD/weakpointer.h

SOURCES += \
    $$PWD/codelocation.cpp \
    $$PWD/error.cpp \
    $$PWD/fileinfo.cpp \
    $$PWD/persistence.cpp \
    $$PWD/scannerpluginmanager.cpp \
    $$PWD/scripttools.cpp \
    $$PWD/settings.cpp \
    $$PWD/preferences.cpp \
    $$PWD/profile.cpp \
    $$PWD/progressobserver.cpp \
    $$PWD/buildoptions.cpp \
    $$PWD/installoptions.cpp \
    $$PWD/setupprojectparameters.cpp

win32 {
    SOURCES += $$PWD/filetime_win.cpp
}

unix {
    SOURCES += $$PWD/filetime_unix.cpp
}
