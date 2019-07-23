include(../../plugins.pri)
include(../../../shared/json/json.pri)

TARGET = keiluvgenerator

QT = core

# Plugin file.

SOURCES += \
    $$PWD/keiluvgeneratorplugin.cpp \

# Common files.

HEADERS += \
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
    $$PWD/keiluvversioninfo.cpp \
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
