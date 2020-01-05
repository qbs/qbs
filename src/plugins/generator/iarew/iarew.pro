include(iarew.pri)
include(../../plugins.pri)
include(../../../shared/json/json.pri)

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
    $$PWD/iarewsettingspropertygroup.h \
    $$PWD/iarewsourcefilepropertygroup.h \
    $$PWD/iarewsourcefilespropertygroup.h \
    $$PWD/iarewtoolchainpropertygroup.h \
    $$PWD/iarewutils.h \
    $$PWD/iarewversioninfo.h \
    $$PWD/iarewworkspace.h \
    $$PWD/iarewworkspacewriter.h

SOURCES += \
    $$PWD/iarewfileversionproperty.cpp \
    $$PWD/iarewgenerator.cpp \
    $$PWD/iarewoptionpropertygroup.cpp \
    $$PWD/iarewproject.cpp \
    $$PWD/iarewprojectwriter.cpp \
    $$PWD/iarewsettingspropertygroup.cpp \
    $$PWD/iarewsourcefilepropertygroup.cpp \
    $$PWD/iarewsourcefilespropertygroup.cpp \
    $$PWD/iarewtoolchainpropertygroup.cpp \
    $$PWD/iarewutils.cpp \
    $$PWD/iarewworkspace.cpp \
    $$PWD/iarewworkspacewriter.cpp

# For ARM architecture.

HEADERS += \
    $$PWD/archs/arm/armarchiversettingsgroup_v8.h \
    $$PWD/archs/arm/armassemblersettingsgroup_v8.h \
    $$PWD/archs/arm/armbuildconfigurationgroup_v8.h \
    $$PWD/archs/arm/armcompilersettingsgroup_v8.h \
    $$PWD/archs/arm/armgeneralsettingsgroup_v8.h \
    $$PWD/archs/arm/armlinkersettingsgroup_v8.h

SOURCES += \
    $$PWD/archs/arm/armarchiversettingsgroup_v8.cpp \
    $$PWD/archs/arm/armassemblersettingsgroup_v8.cpp \
    $$PWD/archs/arm/armbuildconfigurationgroup_v8.cpp \
    $$PWD/archs/arm/armcompilersettingsgroup_v8.cpp \
    $$PWD/archs/arm/armgeneralsettingsgroup_v8.cpp \
    $$PWD/archs/arm/armlinkersettingsgroup_v8.cpp

# For AVR architecture.

HEADERS += \
    $$PWD/archs/avr/avrarchiversettingsgroup_v7.h \
    $$PWD/archs/avr/avrassemblersettingsgroup_v7.h \
    $$PWD/archs/avr/avrbuildconfigurationgroup_v7.h \
    $$PWD/archs/avr/avrcompilersettingsgroup_v7.h \
    $$PWD/archs/avr/avrgeneralsettingsgroup_v7.h \
    $$PWD/archs/avr/avrlinkersettingsgroup_v7.h

SOURCES += \
    $$PWD/archs/avr/avrarchiversettingsgroup_v7.cpp \
    $$PWD/archs/avr/avrassemblersettingsgroup_v7.cpp \
    $$PWD/archs/avr/avrbuildconfigurationgroup_v7.cpp \
    $$PWD/archs/avr/avrcompilersettingsgroup_v7.cpp \
    $$PWD/archs/avr/avrgeneralsettingsgroup_v7.cpp \
    $$PWD/archs/avr/avrlinkersettingsgroup_v7.cpp

# For MCS51 architecture.

HEADERS += \
    $$PWD/archs/mcs51/mcs51archiversettingsgroup_v10.h \
    $$PWD/archs/mcs51/mcs51assemblersettingsgroup_v10.h \
    $$PWD/archs/mcs51/mcs51buildconfigurationgroup_v10.h \
    $$PWD/archs/mcs51/mcs51compilersettingsgroup_v10.h \
    $$PWD/archs/mcs51/mcs51generalsettingsgroup_v10.h \
    $$PWD/archs/mcs51/mcs51linkersettingsgroup_v10.h

SOURCES += \
    $$PWD/archs/mcs51/mcs51archiversettingsgroup_v10.cpp \
    $$PWD/archs/mcs51/mcs51assemblersettingsgroup_v10.cpp \
    $$PWD/archs/mcs51/mcs51buildconfigurationgroup_v10.cpp \
    $$PWD/archs/mcs51/mcs51compilersettingsgroup_v10.cpp \
    $$PWD/archs/mcs51/mcs51generalsettingsgroup_v10.cpp \
    $$PWD/archs/mcs51/mcs51linkersettingsgroup_v10.cpp

# For STM8 architecture.

HEADERS += \
    $$PWD/archs/stm8/stm8archiversettingsgroup_v3.h \
    $$PWD/archs/stm8/stm8assemblersettingsgroup_v3.h \
    $$PWD/archs/stm8/stm8buildconfigurationgroup_v3.h \
    $$PWD/archs/stm8/stm8compilersettingsgroup_v3.h \
    $$PWD/archs/stm8/stm8generalsettingsgroup_v3.h \
    $$PWD/archs/stm8/stm8linkersettingsgroup_v3.h

SOURCES += \
    $$PWD/archs/stm8/stm8archiversettingsgroup_v3.cpp \
    $$PWD/archs/stm8/stm8assemblersettingsgroup_v3.cpp \
    $$PWD/archs/stm8/stm8buildconfigurationgroup_v3.cpp \
    $$PWD/archs/stm8/stm8compilersettingsgroup_v3.cpp \
    $$PWD/archs/stm8/stm8generalsettingsgroup_v3.cpp \
    $$PWD/archs/stm8/stm8linkersettingsgroup_v3.cpp

# For MSP430 architecture.

HEADERS += \
    $$PWD/archs/msp430/msp430archiversettingsgroup_v7.h \
    $$PWD/archs/msp430/msp430assemblersettingsgroup_v7.h \
    $$PWD/archs/msp430/msp430buildconfigurationgroup_v7.h \
    $$PWD/archs/msp430/msp430compilersettingsgroup_v7.h \
    $$PWD/archs/msp430/msp430generalsettingsgroup_v7.h \
    $$PWD/archs/msp430/msp430linkersettingsgroup_v7.h

SOURCES += \
    $$PWD/archs/msp430/msp430archiversettingsgroup_v7.cpp \
    $$PWD/archs/msp430/msp430assemblersettingsgroup_v7.cpp \
    $$PWD/archs/msp430/msp430buildconfigurationgroup_v7.cpp \
    $$PWD/archs/msp430/msp430compilersettingsgroup_v7.cpp \
    $$PWD/archs/msp430/msp430generalsettingsgroup_v7.cpp \
    $$PWD/archs/msp430/msp430linkersettingsgroup_v7.cpp
