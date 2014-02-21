include(../app.pri)
include($${PWD}/../../lib/qtprofilesetup/use_qtprofilesetup.pri)

TARGET = qbs-setup-qt

SOURCES += \
    commandlineparser.cpp \
    main.cpp \
    setupqt.cpp

HEADERS += \
    commandlineparser.h \
    setupqt.h \
    ../shared/qbssettings.h

mingw {
    RC_FILE = qbs-setup-qt.rc
}
