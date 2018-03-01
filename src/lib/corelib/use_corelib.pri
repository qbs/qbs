include(../../../qbs_version.pri)
include(../../library_dirname.pri)

isEmpty(QBSLIBDIR) {
    QBSLIBDIR = $$OUT_PWD/../../../$${QBS_LIBRARY_DIRNAME}
}

unix {
    LIBS += -L$$QBSLIBDIR -lqbscore
}

isEmpty(QBS_RPATH): QBS_RPATH = ../$$QBS_LIBRARY_DIRNAME
!qbs_disable_rpath {
    linux-*: QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,\$\$ORIGIN/$${QBS_RPATH}\'
    macx: QMAKE_LFLAGS += -Wl,-rpath,@loader_path/$${QBS_RPATH}
}

!CONFIG(static, static|shared) {
    QBSCORELIBSUFFIX = $$QBS_VERSION_MAJ
}

win32 {
    CONFIG(debug, debug|release) {
        QBSCORELIB = qbscored$$QBSCORELIBSUFFIX
    }
    CONFIG(release, debug|release) {
        QBSCORELIB = qbscore$$QBSCORELIBSUFFIX
    }
    msvc {
        LIBS += /LIBPATH:$$QBSLIBDIR
        QBSCORELIB = $${QBSCORELIB}.lib
        LIBS += Shell32.lib
    } else {
        LIBS += -L$${QBSLIBDIR}
        QBSCORELIB = lib$${QBSCORELIB}
    }
    LIBS += $$QBSCORELIB
}

INCLUDEPATH += \
    $$PWD

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
qbs_enable_project_file_updates:DEFINES += QBS_ENABLE_PROJECT_FILE_UPDATES
qbs_enable_unit_tests:DEFINES += QBS_ENABLE_UNIT_TESTS
