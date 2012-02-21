INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/sourceproject.h \
    $$PWD/buildproject.h \
    $$PWD/error.h \
    $$PWD/buildproduct.h \
    $$PWD/buildexecutor.h \
    $$PWD/processoutput.h \
    $$PWD/ilogsink.h \
    $$PWD/globals.h \
    $$PWD/mainthreadcommunication.h \
    $$PWD/logmessageevent.h \
    $$PWD/processoutputevent.h \
    $$PWD/runenvironment.h \
    $$PWD/private/resolvedproduct.h

SOURCES += \
    $$PWD/sourceproject.cpp \
    $$PWD/buildproject.cpp \
    $$PWD/qbserror.cpp \
    $$PWD/buildproduct.cpp \
    $$PWD/buildexecutor.cpp \
    $$PWD/processoutput.cpp \
    $$PWD/ilogsink.cpp \
    $$PWD/mainthreadcommunication.cpp \
    $$PWD/logmessageevent.cpp \
    $$PWD/processoutputevent.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/private/resolvedproduct.cpp
