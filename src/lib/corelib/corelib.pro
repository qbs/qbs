TARGET = qbscore
include(../library.pri)

QT += core-private network script
qbs_enable_unit_tests:QT += testlib
qbs_enable_project_file_updates: QT += gui

INCLUDEPATH += $$PWD

CONFIG += depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS

DEFINES += SRCDIR=\\\"$$PWD\\\"

include(api/api.pri)
include(buildgraph/buildgraph.pri)
include(generators/generators.pri)
include(jsextensions/jsextensions.pri)
include(language/language.pri)
include(logging/logging.pri)
include(parser/parser.pri)
include(tools/tools.pri)

win32:LIBS += -lpsapi -lshell32

HEADERS += \
    qbs.h

!qbs_no_dev_install {
    qbs_h.files = qbs.h
    qbs_h.path = $${QBS_INSTALL_PREFIX}/include/qbs
    use_pri.files = use_installed_corelib.pri ../../../qbs_version.pri
    use_pri.path = $${qbs_h.path}
    INSTALLS += qbs_h use_pri
}
