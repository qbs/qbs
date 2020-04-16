import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    Depends { name: "qbsjson" }

    name: "keiluvgenerator"

    files: ["keiluvgeneratorplugin.cpp"]

    Group {
        name: "KEIL UV generator common"
        files: [
            "keiluvfilesgroupspropertygroup.cpp",
            "keiluvfilesgroupspropertygroup.h",
            "keiluvgenerator.cpp",
            "keiluvgenerator.h",
            "keiluvproject.cpp",
            "keiluvproject.h",
            "keiluvprojectwriter.cpp",
            "keiluvprojectwriter.h",
            "keiluvutils.cpp",
            "keiluvutils.h",
            "keiluvversioninfo.h",
            "keiluvworkspace.cpp",
            "keiluvworkspace.h",
            "keiluvworkspacewriter.cpp",
            "keiluvworkspacewriter.h",
        ]
    }
    Group {
        name: "KEIL UV generator for MCS51"
        prefix: "archs/mcs51/"
        files: [
            "mcs51buildtargetgroup_v5.cpp",
            "mcs51buildtargetgroup_v5.h",
            "mcs51commonpropertygroup_v5.cpp",
            "mcs51commonpropertygroup_v5.h",
            "mcs51debugoptiongroup_v5.cpp",
            "mcs51debugoptiongroup_v5.h",
            "mcs51dlloptiongroup_v5.cpp",
            "mcs51dlloptiongroup_v5.h",
            "mcs51targetassemblergroup_v5.cpp",
            "mcs51targetassemblergroup_v5.h",
            "mcs51targetcommonoptionsgroup_v5.cpp",
            "mcs51targetcommonoptionsgroup_v5.h",
            "mcs51targetcompilergroup_v5.cpp",
            "mcs51targetcompilergroup_v5.h",
            "mcs51targetgroup_v5.cpp",
            "mcs51targetgroup_v5.h",
            "mcs51targetlinkergroup_v5.cpp",
            "mcs51targetlinkergroup_v5.h",
            "mcs51targetmiscgroup_v5.cpp",
            "mcs51targetmiscgroup_v5.h",
            "mcs51utilitiesgroup_v5.cpp",
            "mcs51utilitiesgroup_v5.h",
            "mcs51utils.cpp",
            "mcs51utils.h",
        ]
    }
    Group {
        name: "KEIL UV generator for ARM"
        prefix: "archs/arm/"
        files: [
            "armbuildtargetgroup_v5.cpp",
            "armbuildtargetgroup_v5.h",
            "armcommonpropertygroup_v5.cpp",
            "armcommonpropertygroup_v5.h",
            "armdebugoptiongroup_v5.cpp",
            "armdebugoptiongroup_v5.h",
            "armdlloptiongroup_v5.cpp",
            "armdlloptiongroup_v5.h",
            "armtargetassemblergroup_v5.cpp",
            "armtargetassemblergroup_v5.h",
            "armtargetcommonoptionsgroup_v5.cpp",
            "armtargetcommonoptionsgroup_v5.h",
            "armtargetcompilergroup_v5.cpp",
            "armtargetcompilergroup_v5.h",
            "armtargetgroup_v5.cpp",
            "armtargetgroup_v5.h",
            "armtargetlinkergroup_v5.cpp",
            "armtargetlinkergroup_v5.h",
            "armtargetmiscgroup_v5.cpp",
            "armtargetmiscgroup_v5.h",
            "armutilitiesgroup_v5.cpp",
            "armutilitiesgroup_v5.h",
        ]
    }
}
