import qbs

Project {
    name: "qbs plugins"
    references: [
        "generator/clangcompilationdb/clangcompilationdb.qbs",
        "generator/visualstudio/visualstudio.qbs",
        "scanner/cpp/cpp.qbs",
        "scanner/qt/qt.qbs"
    ]
}
