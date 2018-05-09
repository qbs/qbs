import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    name: "makefilegenerator"
    files: [
        "makefilegenerator.cpp",
        "makefilegenerator.h",
        "makefilegeneratorplugin.cpp",
    ]
}
