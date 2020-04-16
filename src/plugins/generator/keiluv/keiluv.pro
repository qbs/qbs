include(keiluv.pri)
include(../../plugins.pri)
include(../../../shared/json/json.pri)

QT = core

# Plugin file.

SOURCES += \
    $$PWD/keiluvgeneratorplugin.cpp \

# Common files.

HEADERS += \
    $$PWD/keiluvconstants.h \
    $$PWD/keiluvfilesgroupspropertygroup.h \
    $$PWD/keiluvgenerator.h \
    $$PWD/keiluvproject.h \
    $$PWD/keiluvprojectwriter.h \
    $$PWD/keiluvutils.h \
    $$PWD/keiluvversioninfo.h \
    $$PWD/keiluvworkspace.h \
    $$PWD/keiluvworkspacewriter.h

SOURCES += \
    $$PWD/keiluvfilesgroupspropertygroup.cpp \
    $$PWD/keiluvgenerator.cpp \
    $$PWD/keiluvproject.cpp \
    $$PWD/keiluvprojectwriter.cpp \
    $$PWD/keiluvutils.cpp \
    $$PWD/keiluvworkspace.cpp \
    $$PWD/keiluvworkspacewriter.cpp

# For MCS51 architecture.

HEADERS += \
    $$PWD/archs/mcs51/mcs51buildtargetgroup_v5.h \
    $$PWD/archs/mcs51/mcs51commonpropertygroup_v5.h \
    $$PWD/archs/mcs51/mcs51debugoptiongroup_v5.h \
    $$PWD/archs/mcs51/mcs51dlloptiongroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetassemblergroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetcommonoptionsgroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetcompilergroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetgroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetlinkergroup_v5.h \
    $$PWD/archs/mcs51/mcs51targetmiscgroup_v5.h \
    $$PWD/archs/mcs51/mcs51utilitiesgroup_v5.h \
    $$PWD/archs/mcs51/mcs51utils.h

SOURCES += \
    $$PWD/archs/mcs51/mcs51buildtargetgroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51commonpropertygroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51debugoptiongroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51dlloptiongroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetassemblergroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetcommonoptionsgroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetcompilergroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetgroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetlinkergroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51targetmiscgroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51utilitiesgroup_v5.cpp \
    $$PWD/archs/mcs51/mcs51utils.cpp

# For ARM architecture.

HEADERS += \
    $$PWD/archs/arm/armbuildtargetgroup_v5.h \
    $$PWD/archs/arm/armcommonpropertygroup_v5.h \
    $$PWD/archs/arm/armdebugoptiongroup_v5.h \
    $$PWD/archs/arm/armdlloptiongroup_v5.h \
    $$PWD/archs/arm/armtargetassemblergroup_v5.h \
    $$PWD/archs/arm/armtargetcommonoptionsgroup_v5.h \
    $$PWD/archs/arm/armtargetcompilergroup_v5.h \
    $$PWD/archs/arm/armtargetgroup_v5.h \
    $$PWD/archs/arm/armtargetlinkergroup_v5.h \
    $$PWD/archs/arm/armtargetmiscgroup_v5.h \
    $$PWD/archs/arm/armutilitiesgroup_v5.h

SOURCES += \
    $$PWD/archs/arm/armbuildtargetgroup_v5.cpp \
    $$PWD/archs/arm/armcommonpropertygroup_v5.cpp \
    $$PWD/archs/arm/armdebugoptiongroup_v5.cpp \
    $$PWD/archs/arm/armdlloptiongroup_v5.cpp \
    $$PWD/archs/arm/armtargetassemblergroup_v5.cpp \
    $$PWD/archs/arm/armtargetcommonoptionsgroup_v5.cpp \
    $$PWD/archs/arm/armtargetcompilergroup_v5.cpp \
    $$PWD/archs/arm/armtargetgroup_v5.cpp \
    $$PWD/archs/arm/armtargetlinkergroup_v5.cpp \
    $$PWD/archs/arm/armtargetmiscgroup_v5.cpp \
    $$PWD/archs/arm/armutilitiesgroup_v5.cpp
