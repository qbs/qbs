INCLUDEPATH += $$PWD/../.. # for plugins

HEADERS += \
    $$PWD/codelocation.h \
    $$PWD/error.h \
    $$PWD/fileinfo.h \
    $$PWD/filetime.h \
    $$PWD/id.h \
    $$PWD/persistence.h \
    $$PWD/scannerpluginmanager.h \
    $$PWD/scripttools.h \
    $$PWD/settings.h \
    $$PWD/preferences.h \
    $$PWD/profile.h \
    $$PWD/processresult.h \
    $$PWD/processresult_p.h \
    $$PWD/progressobserver.h \
    $$PWD/propertyfinder.h \
    $$PWD/hostosinfo.h \
    $$PWD/buildoptions.h \
    $$PWD/installoptions.h \
    $$PWD/cleanoptions.h \
    $$PWD/setupprojectparameters.h \
    $$PWD/persistentobject.h \
    $$PWD/weakpointer.h \
    $$PWD/qbs_export.h \
    $$PWD/qbsassert.h \
    $$PWD/qttools.h

SOURCES += \
    $$PWD/codelocation.cpp \
    $$PWD/error.cpp \
    $$PWD/fileinfo.cpp \
    $$PWD/id.cpp \
    $$PWD/persistence.cpp \
    $$PWD/scannerpluginmanager.cpp \
    $$PWD/scripttools.cpp \
    $$PWD/settings.cpp \
    $$PWD/preferences.cpp \
    $$PWD/processresult.cpp \
    $$PWD/profile.cpp \
    $$PWD/progressobserver.cpp \
    $$PWD/propertyfinder.cpp \
    $$PWD/buildoptions.cpp \
    $$PWD/installoptions.cpp \
    $$PWD/cleanoptions.cpp \
    $$PWD/setupprojectparameters.cpp \
    $$PWD/qbsassert.cpp \
    $$PWD/qttools.cpp

win32 {
    SOURCES += $$PWD/filetime_win.cpp
}

unix {
    SOURCES += $$PWD/filetime_unix.cpp
}

all_tests {
    HEADERS += $$PWD/tst_tools.h
    SOURCES += $$PWD/tst_tools.cpp
}

!qbs_no_dev_install {
    tools_headers.files = \
        $$PWD/cleanoptions.h \
        $$PWD/codelocation.h \
        $$PWD/error.h \
        $$PWD/settings.h \
        $$PWD/preferences.h \
        $$PWD/profile.h \
        $$PWD/processresult.h \
        $$PWD/buildoptions.h \
        $$PWD/installoptions.h \
        $$PWD/setupprojectparameters.h
    tools_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/tools
    INSTALLS += tools_headers
}
