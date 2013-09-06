QT += script xml

QBSLIBDIR=$${PWD}/../../lib
unix {
    LIBS += -L$$QBSLIBDIR -lqbscore
}

!disable_rpath:unix:QMAKE_LFLAGS += -Wl,-rpath,$${QBSLIBDIR}

win32 {
    CONFIG(debug, debug|release) {
        QBSCORELIB = qbscored
    }
    CONFIG(release, debug|release) {
        QBSCORELIB = qbscore
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
