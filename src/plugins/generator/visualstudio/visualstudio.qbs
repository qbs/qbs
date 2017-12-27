import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    Depends { name: "qbsjson" }
    Depends { name: "qbsmsbuild" }

    name: "visualstudiogenerator"

    files: ["visualstudiogeneratorplugin.cpp"]

    Group {
        name: "Visual Studio generator"
        files: [
            "msbuildfiltersproject.cpp",
            "msbuildfiltersproject.h",
            "msbuildqbsgenerateproject.cpp",
            "msbuildqbsgenerateproject.h",
            "msbuildqbsproductproject.cpp",
            "msbuildqbsproductproject.h",
            "msbuildsharedsolutionpropertiesproject.cpp",
            "msbuildsharedsolutionpropertiesproject.h",
            "msbuildsolutionpropertiesproject.cpp",
            "msbuildsolutionpropertiesproject.h",
            "msbuildtargetproject.cpp",
            "msbuildtargetproject.h",
            "msbuildutils.h",
            "visualstudiogenerator.cpp",
            "visualstudiogenerator.h",
            "visualstudioguidpool.cpp",
            "visualstudioguidpool.h",
        ]
    }
}
