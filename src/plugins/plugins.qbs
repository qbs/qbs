Project {
    name: "qbs plugins"
    references: [
        "generator/graphviz/graphviz.qbs",
        "generator/clangcompilationdb/clangcompilationdb.qbs",
        "generator/makefilegenerator/makefilegenerator.qbs",
        "generator/visualstudio/visualstudio.qbs",
        "generator/iarew/iarew.qbs",
        "generator/keiluv/keiluv.qbs",
        "scanner/cpp/cpp.qbs",
        "scanner/qt/qt.qbs"
    ]
}
