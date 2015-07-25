QT += xml

HEADERS += \
    $$PWD/file.h \
    $$PWD/temporarydir.h \
    $$PWD/textfile.h \
    $$PWD/process.h \
    $$PWD/moduleproperties.h \
    $$PWD/domxml.h \
    $$PWD/jsextensions.h

SOURCES += \
    $$PWD/file.cpp \
    $$PWD/temporarydir.cpp \
    $$PWD/textfile.cpp \
    $$PWD/process.cpp \
    $$PWD/moduleproperties.cpp \
    $$PWD/domxml.cpp \
    $$PWD/jsextensions.cpp

mac {
    HEADERS += $$PWD/propertylist.h $$PWD/propertylistutils.h
    OBJECTIVE_SOURCES += $$PWD/propertylist.mm $$PWD/propertylistutils.mm
    LIBS += -framework Foundation
}
