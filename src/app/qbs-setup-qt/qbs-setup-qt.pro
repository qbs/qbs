include(../app.pri)

TARGET = qbs-setup-qt

SOURCES += \
    commandlineparser.cpp \
    main.cpp \
    setupqt.cpp

HEADERS += \
    commandlineparser.h \
    setupqt.h

mingw {
    RC_FILE = qbs-setup-qt.rc
}
