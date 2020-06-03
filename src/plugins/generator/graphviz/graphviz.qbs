import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    name: "graphvizgenerator"
    files: [
        "dotgraph.cpp",
        "dotgraph.h",
        "graphvizgenerator.cpp",
        "graphvizgenerator.h",
        "graphvizgeneratorplugin.cpp",
    ]
}
