TARGET = qbscore
include(../library.pri)

QT += script gui
all_tests:QT += testlib

INCLUDEPATH += $$PWD

CONFIG += depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS

DEFINES += SRCDIR=\\\"$$PWD\\\"

include(api/api.pri)
include(buildgraph/buildgraph.pri)
include(jsextensions/jsextensions.pri)
include(language/language.pri)
include(logging/logging.pri)
include(parser/parser.pri)
include(tools/tools.pri)

HEADERS += \
    qbs.h

!qbs_no_dev_install {
    qbs_h.files = qbs.h
    qbs_h.path = $${QBS_INSTALL_PREFIX}/include/qbs
    use_pri.files = use_installed_corelib.pri ../../../qbs_version.pri
    use_pri.path = $${qbs_h.path}
    INSTALLS += qbs_h use_pri
}
