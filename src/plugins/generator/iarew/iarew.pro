include(../../plugins.pri)
include(../../../shared/json/json.pri)

TARGET = iarewgenerator

QT = core

# Plugin file.

SOURCES += \
    $$PWD/iarewgeneratorplugin.cpp \

# Common files.

HEADERS += \
    $$PWD/iarewfileversionproperty.h \
    $$PWD/iarewgenerator.h \
    $$PWD/iarewoptionpropertygroup.h \
    $$PWD/iarewproject.h \
    $$PWD/iarewprojectwriter.h \
    $$PWD/iarewproperty.h \
    $$PWD/iarewpropertygroup.h \
    $$PWD/iarewsettingspropertygroup.h \
    $$PWD/iarewsourcefilepropertygroup.h \
    $$PWD/iarewsourcefilespropertygroup.h \
    $$PWD/iarewtoolchainpropertygroup.h \
    $$PWD/iarewutils.h \
    $$PWD/iarewversioninfo.h \
    $$PWD/iarewworkspace.h \
    $$PWD/iarewworkspacewriter.h \
    $$PWD/iiarewnodevisitor.h \

SOURCES += \
    $$PWD/iarewfileversionproperty.cpp \
    $$PWD/iarewgenerator.cpp \
    $$PWD/iarewoptionpropertygroup.cpp \
    $$PWD/iarewproject.cpp \
    $$PWD/iarewprojectwriter.cpp \
    $$PWD/iarewproperty.cpp \
    $$PWD/iarewpropertygroup.cpp \
    $$PWD/iarewsettingspropertygroup.cpp \
    $$PWD/iarewsourcefilepropertygroup.cpp \
    $$PWD/iarewsourcefilespropertygroup.cpp \
    $$PWD/iarewtoolchainpropertygroup.cpp \
    $$PWD/iarewutils.cpp \
    $$PWD/iarewversioninfo.cpp \
    $$PWD/iarewworkspace.cpp \
    $$PWD/iarewworkspacewriter.cpp \

# For ARM architecture.

HEADERS += \
    $$PWD/archs/arm/armarchiversettingsgroup_v8.h \
    $$PWD/archs/arm/armassemblersettingsgroup_v8.h \
    $$PWD/archs/arm/armbuildconfigurationgroup_v8.h \
    $$PWD/archs/arm/armcompilersettingsgroup_v8.h \
    $$PWD/archs/arm/armgeneralsettingsgroup_v8.h \
    $$PWD/archs/arm/armlinkersettingsgroup_v8.h \

SOURCES += \
    $$PWD/archs/arm/armarchiversettingsgroup_v8.cpp \
    $$PWD/archs/arm/armassemblersettingsgroup_v8.cpp \
    $$PWD/archs/arm/armbuildconfigurationgroup_v8.cpp \
    $$PWD/archs/arm/armcompilersettingsgroup_v8.cpp \
    $$PWD/archs/arm/armgeneralsettingsgroup_v8.cpp \
    $$PWD/archs/arm/armlinkersettingsgroup_v8.cpp \
