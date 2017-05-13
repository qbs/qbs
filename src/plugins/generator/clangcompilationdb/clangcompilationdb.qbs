import qbs
import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    name: "clangcompilationdbgenerator"
    files: [
        "clangcompilationdbgenerator.cpp",
        "clangcompilationdbgenerator.h",
        "clangcompilationdbgeneratorplugin.cpp"
    ]
}
