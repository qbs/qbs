QT += xml

HEADERS += \
    $$PWD/moduleproperties.h \
    $$PWD/jsextensions.h \
    $$PWD/jsextensions_p.h

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
    HEADERS += $$PWD/propertylistutils.h
    OBJECTIVE_SOURCES += $$PWD/propertylist.mm $$PWD/propertylistutils.mm
    LIBS += -framework Foundation
} else {
    SOURCES += $$PWD/propertylist.cpp
}
