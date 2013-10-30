include(../../qbs_version.pri)

isEmpty(QBSLIBDIR) {
    QBSLIBDIR = $$OUT_PWD/../../../lib
}

QT += script xml

unix {
    LIBS += -L$$QBSLIBDIR -lqbscore
}

!disable_rpath {
    linux-*:QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,\$\$ORIGIN/../lib\'
    macx:QMAKE_LFLAGS += -Wl,-rpath,@loader_path/../lib
}

win32 {
    CONFIG(debug, debug|release) {
        QBSCORELIB = qbscored$$QBS_VERSION_MAJ
    }
    CONFIG(release, debug|release) {
        QBSCORELIB = qbscore$$QBS_VERSION_MAJ
    }
    win32-msvc* {
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

CONFIG += depend_includepath

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
