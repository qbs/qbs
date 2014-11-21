include(../app.pri)

TARGET = qbs-setup-android

SOURCES += \
    android-setup.cpp \
    commandlineparser.cpp \
    main.cpp

HEADERS += \
    android-setup.h \
    commandlineparser.h
