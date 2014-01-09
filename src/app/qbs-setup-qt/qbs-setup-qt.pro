include(../app.pri)

TARGET = qbs-setup-qt

SOURCES += \
    main.cpp \
    setupqt.cpp \
    setupqtprofile.cpp

HEADERS += \
    setupqt.h \
    setupqtprofile.h \
    ../shared/qbssettings.h
