import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    Depends { name: "qbsjson" }

    name: "iarewgenerator"

    files: ["iarewgeneratorplugin.cpp"]

    Group {
        name: "IAR EW generator common"
        files: [
            "iarewfileversionproperty.cpp",
            "iarewfileversionproperty.h",
            "iarewgenerator.cpp",
            "iarewgenerator.h",
            "iarewoptionpropertygroup.cpp",
            "iarewoptionpropertygroup.h",
            "iarewproject.cpp",
            "iarewproject.h",
            "iarewprojectwriter.cpp",
            "iarewprojectwriter.h",
            "iarewproperty.cpp",
            "iarewproperty.h",
            "iarewpropertygroup.cpp",
            "iarewpropertygroup.h",
            "iarewsettingspropertygroup.cpp",
            "iarewsettingspropertygroup.h",
            "iarewsourcefilepropertygroup.cpp",
            "iarewsourcefilepropertygroup.h",
            "iarewsourcefilespropertygroup.cpp",
            "iarewsourcefilespropertygroup.h",
            "iarewtoolchainpropertygroup.cpp",
            "iarewtoolchainpropertygroup.h",
            "iarewutils.cpp",
            "iarewutils.h",
            "iarewversioninfo.cpp",
            "iarewversioninfo.h",
            "iarewworkspace.cpp",
            "iarewworkspace.h",
            "iarewworkspacewriter.cpp",
            "iarewworkspacewriter.h",
            "iiarewnodevisitor.h",
        ]
    }
    Group {
        name: "IAR EW generator for ARM"
        prefix: "archs/arm/"
        files: [
            "armarchiversettingsgroup_v8.cpp",
            "armarchiversettingsgroup_v8.h",
            "armassemblersettingsgroup_v8.cpp",
            "armassemblersettingsgroup_v8.h",
            "armbuildconfigurationgroup_v8.cpp",
            "armbuildconfigurationgroup_v8.h",
            "armcompilersettingsgroup_v8.cpp",
            "armcompilersettingsgroup_v8.h",
            "armgeneralsettingsgroup_v8.cpp",
            "armgeneralsettingsgroup_v8.h",
            "armlinkersettingsgroup_v8.cpp",
            "armlinkersettingsgroup_v8.h",
        ]
    }
}
