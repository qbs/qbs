TARGET = qbscore
include(../library.pri)

isEmpty(QBS_RELATIVE_LIBEXEC_PATH) {
    win32:QBS_RELATIVE_LIBEXEC_PATH=../bin
    else:QBS_RELATIVE_LIBEXEC_PATH=../libexec/qbs
}
DEFINES += QBS_RELATIVE_LIBEXEC_PATH=\\\"$${QBS_RELATIVE_LIBEXEC_PATH}\\\"

QT += core-private network script
qbs_enable_project_file_updates: QT += gui

INCLUDEPATH += $$PWD

CONFIG += depend_includepath

DEFINES += SRCDIR=\\\"$$PWD\\\"

include(api/api.pri)
include(buildgraph/buildgraph.pri)
include(generators/generators.pri)
include(jsextensions/jsextensions.pri)
include(language/language.pri)
include(logging/logging.pri)
include(parser/parser.pri)
include(tools/tools.pri)

CONFIG(static, static|shared) {
    !isEmpty(QBS_PLUGINS_BUILD_DIR) {
        destdirPrefix = $${QBS_PLUGINS_BUILD_DIR}
    } else {
        destdirPrefix = $$shadowed($$PWD)/../../../$${QBS_LIBRARY_DIRNAME}
    }
    LIBS += -L$${destdirPrefix}/qbs/plugins -lqbs_cpp_scanner -lqbs_qt_scanner
}

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
