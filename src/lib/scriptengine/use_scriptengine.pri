!qbs_do_not_link_bundled_qtscript {
    include(../../library_dirname.pri)
    isEmpty(QBSLIBDIR) {
        QBSLIBDIR = $$shadowed($$PWD/../../../$${QBS_LIBRARY_DIRNAME})
    }
    LIBS += -L$$QBSLIBDIR -lqbsscriptengine$$qtPlatformTargetSuffix()
}

INCLUDEPATH += \
    $$shadowed($$PWD/include)
