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

INCLUDEPATH += \
    $$PWD

CONFIG += depend_includepath

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
