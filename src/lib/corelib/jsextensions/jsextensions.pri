QT += xml

HEADERS += \
    $$PWD/environmentextension.h \
    $$PWD/file.h \
    $$PWD/fileinfoextension.h \
    $$PWD/temporarydir.h \
    $$PWD/textfile.h \
    $$PWD/process.h \
    $$PWD/moduleproperties.h \
    $$PWD/domxml.h \
    $$PWD/jsextensions.h \
    $$PWD/utilitiesextension.h

SOURCES += \
    $$PWD/environmentextension.cpp \
    $$PWD/file.cpp \
    $$PWD/fileinfoextension.cpp \
    $$PWD/temporarydir.cpp \
    $$PWD/textfile.cpp \
    $$PWD/process.cpp \
    $$PWD/moduleproperties.cpp \
    $$PWD/domxml.cpp \
    $$PWD/jsextensions.cpp \
    $$PWD/utilitiesextension.cpp

mac {
    HEADERS += $$PWD/propertylist.h $$PWD/propertylistutils.h
    OBJECTIVE_SOURCES += $$PWD/propertylist.mm $$PWD/propertylistutils.mm
    LIBS += -framework Foundation
}
