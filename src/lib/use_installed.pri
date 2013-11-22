include(qbs_version.pri)

QT += script xml

QBSLIBDIR=$${PWD}/../../lib
unix {
    LIBS += -L$$QBSLIBDIR -lqbscore
}

!disable_rpath:unix:QMAKE_LFLAGS += -Wl,-rpath,$${QBSLIBDIR}

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

INCLUDEPATH += $$PWD

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
