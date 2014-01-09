include(../app.pri)
include($${PWD}/../../lib/qtprofilesetup/use_qtprofilesetup.pri)

TARGET = qbs-setup-qt

SOURCES += \
    main.cpp \
    setupqt.cpp

HEADERS += \
    setupqt.h \
    ../shared/qbssettings.h
