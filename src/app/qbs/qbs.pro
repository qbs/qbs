include(../app.pri)
include(parser/parser.pri)

TARGET = qbs

SOURCES += main.cpp \
    ctrlchandler.cpp \
    application.cpp \
    status.cpp \
    consoleprogressobserver.cpp \
    commandlinefrontend.cpp \
    qbstool.cpp

HEADERS += \
    ctrlchandler.h \
    application.h \
    status.h \
    consoleprogressobserver.h \
    commandlinefrontend.h \
    qbstool.h

include(../../library_dirname.pri)
isEmpty(QBS_RELATIVE_LIBEXEC_PATH) {
    win32:QBS_RELATIVE_LIBEXEC_PATH=.
    else:QBS_RELATIVE_LIBEXEC_PATH=../libexec/qbs
}
isEmpty(QBS_RELATIVE_PLUGINS_PATH):QBS_RELATIVE_PLUGINS_PATH=../$${QBS_LIBRARY_DIRNAME}
isEmpty(QBS_RELATIVE_SEARCH_PATH):QBS_RELATIVE_SEARCH_PATH=..
DEFINES += QBS_RELATIVE_LIBEXEC_PATH=\\\"$${QBS_RELATIVE_LIBEXEC_PATH}\\\"
DEFINES += QBS_RELATIVE_PLUGINS_PATH=\\\"$${QBS_RELATIVE_PLUGINS_PATH}\\\"
DEFINES += QBS_RELATIVE_SEARCH_PATH=\\\"$${QBS_RELATIVE_SEARCH_PATH}\\\"
