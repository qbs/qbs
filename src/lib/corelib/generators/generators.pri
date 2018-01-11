include(../../../install_prefix.pri)

SOURCES += \
    $$PWD/generatableprojectiterator.cpp \
    $$PWD/generator.cpp \
    $$PWD/generatordata.cpp

HEADERS += \
    $$PWD/generatableprojectiterator.h \
    $$PWD/generator.h \
    $$PWD/generatordata.h \
    $$PWD/igeneratableprojectvisitor.h

!qbs_no_dev_install {
    generators_headers.files = \
        $$PWD/generator.h \
        $$PWD/generatordata.h
    generators_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/generators
    INSTALLS += generators_headers
}
