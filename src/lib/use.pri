isEmpty(QBSLIBDIR) {
    QBSLIBDIR = $$OUT_PWD/../../../lib
}

QT += script

unix {
    LIBS += -L$$QBSLIBDIR -lqbscore
    POST_TARGETDEPS += $$QBSLIBDIR/libqbscore.a
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
        POST_TARGETDEPS += $$QBSLIBDIR/$$QBSCORELIB
    } else {
        LIBS += -L$${QBSLIBDIR}
        QBSCORELIB = lib$${QBSCORELIB}
    }
    LIBS += $$QBSCORELIB
}

INCLUDEPATH += \
    $$PWD \
    $$PWD/..

DEPENDPATH += \
    $$PWD/buildgraph\
    $$PWD/Qbs\
    $$PWD/l2\
    $$PWD/parser\
    $$PWD/tools\
    $$INCLUDEPATH
