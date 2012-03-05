QT = core script
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent
TEMPLATE = lib
DESTDIR = ../../lib
TARGET = qbscore

CONFIG += staticlib depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS

win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d

include(jsextensions/jsextensions.pri)
include(tools/tools.pri)
include(parser/parser.pri)
include(buildgraph/buildgraph.pri)
include(Qbs/Qbs.pri)
include(language/language.pri)
