!qbs_do_not_link_bundled_qtscript {
    include(../../library_dirname.pri)
    isEmpty(QBSLIBDIR) {
        QBSLIBDIR = $$shadowed($$PWD/../../../$${QBS_LIBRARY_DIRNAME})
    }

    LIBS += -L$$QBSLIBDIR
    macos {
        LIBS += -lqbsscriptengine
    }
    else {
        LIBS += -lqbsscriptengine$$qtPlatformTargetSuffix()
    }

}

INCLUDEPATH += \
    $$PWD/include \
    $$shadowed($$PWD/include)
