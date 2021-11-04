include(../bundledlibs.pri)

qbs_use_bundled_qtscript:!qbs_do_not_link_bundled_qtscript {
    include(../../library_dirname.pri)
    isEmpty(QBSLIBDIR) {
        QBSLIBDIR = $$shadowed($$PWD/../../../$${QBS_LIBRARY_DIRNAME})
    }

    versionAtLeast(QT_VERSION, 6.0.0) {
        QT *= core5compat
    }

    LIBS += -L$$QBSLIBDIR
    macos {
        LIBS += -lqbsscriptengine
    }
    else {
        LIBS += -lqbsscriptengine$$qtPlatformTargetSuffix()
    }

    isEmpty(QBS_RPATH): QBS_RPATH = ../$$QBS_LIBRARY_DIRNAME
    !qbs_disable_rpath {
        linux-*: QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,\$\$ORIGIN/$${QBS_RPATH}\'
        macos: QMAKE_LFLAGS += -Wl,-rpath,@loader_path/$${QBS_RPATH}
    }
}

INCLUDEPATH += \
    $$PWD/include \
    $$shadowed($$PWD/include)
