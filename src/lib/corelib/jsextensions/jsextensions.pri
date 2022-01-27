QT += xml

HEADERS += \
    $$PWD/moduleproperties.h \
    $$PWD/jsextensions.h \
    $$PWD/pkgconfigjs.h

SOURCES += \
    $$PWD/environmentextension.cpp \
    $$PWD/file.cpp \
    $$PWD/fileinfoextension.cpp \
    $$PWD/host.cpp \
    $$PWD/temporarydir.cpp \
    $$PWD/textfile.cpp \
    $$PWD/binaryfile.cpp \
    $$PWD/pkgconfigjs.cpp \
    $$PWD/process.cpp \
    $$PWD/moduleproperties.cpp \
    $$PWD/domxml.cpp \
    $$PWD/jsextensions.cpp \
    $$PWD/utilitiesextension.cpp

darwin {
    HEADERS += $$PWD/propertylistutils.h $$PWD/propertylist_darwin.h
    SOURCES += $$PWD/propertylist_darwin.mm $$PWD/propertylistutils.mm
    LIBS += -framework Foundation
} else {
    SOURCES += $$PWD/propertylist.cpp
}
