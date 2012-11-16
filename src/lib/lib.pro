QT = core script
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent
TEMPLATE = lib
DESTDIR = ../../lib
TARGET = qbscore

CONFIG += static depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS

win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d

include(../../qbs_version.pri)
include(api/api.pri)
include(buildgraph/buildgraph.pri)
include(jsextensions/jsextensions.pri)
include(language/language.pri)
include(logging/logging.pri)
include(parser/parser.pri)
include(tools/tools.pri)

HEADERS += \
    qbs.h
