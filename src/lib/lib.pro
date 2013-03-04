QT = core script testlib
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent
TEMPLATE = lib
DESTDIR = ../../lib
!isEmpty(QBS_DLLDESTDIR):DLLDESTDIR = $${QBS_DLLDESTDIR}
else:DLLDESTDIR = ../../bin
INCLUDEPATH += $$PWD
TARGET = qbscore

CONFIG += shared dll depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS
DEFINES += QBS_LIBRARY
DEFINES += SRCDIR=\\\"$$PWD/\\\"
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols

win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d
win32-msvc*|win32-icc:QMAKE_CXXFLAGS += /WX
else:*g++*|*clang*|*icc*:QMAKE_CXXFLAGS += -Werror
macx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/

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

win32 {
    dlltarget.path = /bin
    INSTALLS += dlltarget
}
target.path = /lib
INSTALLS += target

qbs_h.files = qbs.h
qbs_h.path = /include/qbs
INSTALLS += qbs_h
