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
    } else {
        LIBS += -L$${QBSLIBDIR}
        QBSCORELIB = lib$${QBSCORELIB}
    }
    QBSLIBS += $$QBSCORELIB
    LIBS += Shell32.lib $$QBSLIBS
    for(x, QBSLIBS): POST_TARGETDEPS+=$$QBSLIBDIR/$$x
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
