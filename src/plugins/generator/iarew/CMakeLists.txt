set(COMMON_SOURCES
    iarewfileversionproperty.cpp
    iarewfileversionproperty.h
    iarewgenerator.cpp
    iarewgenerator.h
    iarewgeneratorplugin.cpp
    iarewoptionpropertygroup.cpp
    iarewoptionpropertygroup.h
    iarewproject.cpp
    iarewproject.h
    iarewprojectwriter.cpp
    iarewprojectwriter.h
    iarewsettingspropertygroup.cpp
    iarewsettingspropertygroup.h
    iarewsourcefilepropertygroup.cpp
    iarewsourcefilepropertygroup.h
    iarewsourcefilespropertygroup.cpp
    iarewsourcefilespropertygroup.h
    iarewtoolchainpropertygroup.cpp
    iarewtoolchainpropertygroup.h
    iarewutils.cpp
    iarewutils.h
    iarewversioninfo.h
    iarewworkspace.cpp
    iarewworkspace.h
    iarewworkspacewriter.cpp
    iarewworkspacewriter.h
    )

set(ARCHS_ARM_SOURCES
    armarchiversettingsgroup_v8.cpp
    armarchiversettingsgroup_v8.h
    armassemblersettingsgroup_v8.cpp
    armassemblersettingsgroup_v8.h
    armbuildconfigurationgroup_v8.cpp
    armbuildconfigurationgroup_v8.h
    armcompilersettingsgroup_v8.cpp
    armcompilersettingsgroup_v8.h
    armgeneralsettingsgroup_v8.cpp
    armgeneralsettingsgroup_v8.h
    armlinkersettingsgroup_v8.cpp
    armlinkersettingsgroup_v8.h
    )
list_transform_prepend(ARCHS_ARM_SOURCES archs/arm/)

set(ARCHS_AVR_SOURCES
    avrarchiversettingsgroup_v7.cpp
    avrarchiversettingsgroup_v7.h
    avrassemblersettingsgroup_v7.cpp
    avrassemblersettingsgroup_v7.h
    avrbuildconfigurationgroup_v7.cpp
    avrbuildconfigurationgroup_v7.h
    avrcompilersettingsgroup_v7.cpp
    avrcompilersettingsgroup_v7.h
    avrgeneralsettingsgroup_v7.cpp
    avrgeneralsettingsgroup_v7.h
    avrlinkersettingsgroup_v7.cpp
    avrlinkersettingsgroup_v7.h
    )
list_transform_prepend(ARCHS_AVR_SOURCES archs/avr/)

set(ARCHS_MCS51_SOURCES
    mcs51archiversettingsgroup_v10.cpp
    mcs51archiversettingsgroup_v10.h
    mcs51assemblersettingsgroup_v10.cpp
    mcs51assemblersettingsgroup_v10.h
    mcs51buildconfigurationgroup_v10.cpp
    mcs51buildconfigurationgroup_v10.h
    mcs51compilersettingsgroup_v10.cpp
    mcs51compilersettingsgroup_v10.h
    mcs51generalsettingsgroup_v10.cpp
    mcs51generalsettingsgroup_v10.h
    mcs51linkersettingsgroup_v10.cpp
    mcs51linkersettingsgroup_v10.h
    )
list_transform_prepend(ARCHS_MCS51_SOURCES archs/mcs51/)

set(ARCHS_STM8_SOURCES
    stm8archiversettingsgroup_v3.cpp
    stm8archiversettingsgroup_v3.h
    stm8assemblersettingsgroup_v3.cpp
    stm8assemblersettingsgroup_v3.h
    stm8buildconfigurationgroup_v3.cpp
    stm8buildconfigurationgroup_v3.h
    stm8compilersettingsgroup_v3.cpp
    stm8compilersettingsgroup_v3.h
    stm8generalsettingsgroup_v3.cpp
    stm8generalsettingsgroup_v3.h
    stm8linkersettingsgroup_v3.cpp
    stm8linkersettingsgroup_v3.h
    )
list_transform_prepend(ARCHS_STM8_SOURCES archs/stm8/)

set(ARCHS_MSP430_SOURCES
    msp430archiversettingsgroup_v7.cpp
    msp430archiversettingsgroup_v7.h
    msp430assemblersettingsgroup_v7.cpp
    msp430assemblersettingsgroup_v7.h
    msp430buildconfigurationgroup_v7.cpp
    msp430buildconfigurationgroup_v7.h
    msp430compilersettingsgroup_v7.cpp
    msp430compilersettingsgroup_v7.h
    msp430generalsettingsgroup_v7.cpp
    msp430generalsettingsgroup_v7.h
    msp430linkersettingsgroup_v7.cpp
    msp430linkersettingsgroup_v7.h
    )
list_transform_prepend(ARCHS_MSP430_SOURCES archs/msp430/)

add_qbs_plugin(iarewgenerator
    DEPENDS qbscore qbsjson
    SOURCES
        ${COMMON_SOURCES}
        ${ARCHS_ARM_SOURCES}
        ${ARCHS_AVR_SOURCES}
        ${ARCHS_MCS51_SOURCES}
        ${ARCHS_STM8_SOURCES}
        ${ARCHS_MSP430_SOURCES}
    )
