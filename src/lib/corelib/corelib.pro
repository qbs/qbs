TARGET = qbscore
include(../library.pri)
include(../bundledlibs.pri)

qbs_use_bundled_qtscript {
    include(../scriptengine/use_scriptengine.pri)
} else {
    QT += script
}

isEmpty(QBS_RELATIVE_LIBEXEC_PATH) {
    win32:QBS_RELATIVE_LIBEXEC_PATH=../bin
    else:QBS_RELATIVE_LIBEXEC_PATH=../libexec/qbs
}
DEFINES += QBS_RELATIVE_LIBEXEC_PATH=\\\"$${QBS_RELATIVE_LIBEXEC_PATH}\\\"

QT += core-private network
qbs_enable_project_file_updates: QT += gui

INCLUDEPATH += $$PWD

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
