import qbs

Project {
    name: "qbs plugins"
    references: [
        "generator/clangcompilationdb/clangcompilationdb.qbs",
        "generator/makefilegenerator/makefilegenerator.qbs",
        "generator/visualstudio/visualstudio.qbs",
        "generator/iarew/iarew.qbs",
        "scanner/cpp/cpp.qbs",
        "scanner/qt/qt.qbs"
    ]
}
