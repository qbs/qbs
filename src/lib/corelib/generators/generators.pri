include(../../../install_prefix.pri)

SOURCES += \
    $$PWD/generatableprojectiterator.cpp \
    $$PWD/generator.cpp \
    $$PWD/generatordata.cpp \
    $$PWD/generatorutils.cpp \
    $$PWD/generatorversioninfo.cpp \
    $$PWD/xmlproject.cpp \
    $$PWD/xmlprojectwriter.cpp\
    $$PWD/xmlproperty.cpp \
    $$PWD/xmlpropertygroup.cpp \
    $$PWD/xmlworkspace.cpp \
    $$PWD/xmlworkspacewriter.cpp

HEADERS += \
    $$PWD/generatableprojectiterator.h \
    $$PWD/generator.h \
    $$PWD/generatordata.h \
    $$PWD/generatorutils.h \
    $$PWD/generatorversioninfo.h \
    $$PWD/igeneratableprojectvisitor.h \
    $$PWD/ixmlnodevisitor.h \
    $$PWD/ixmlnodevisitor.h \
    $$PWD/xmlproject.h \
    $$PWD/xmlprojectwriter.h \
    $$PWD/xmlproperty.h \
    $$PWD/xmlpropertygroup.h \
    $$PWD/xmlworkspace.h \
    $$PWD/xmlworkspacewriter.h

!qbs_no_dev_install {
    generators_headers.files = \
        $$PWD/generator.h \
        $$PWD/generatordata.h
    generators_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/generators
    INSTALLS += generators_headers
}
