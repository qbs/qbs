import qbs

Project {
    name: "Autotests"
    references: [
        "api/api.qbs",
        "blackbox/blackbox.qbs",
        "blackbox/blackbox-apple.qbs",
        "blackbox/blackbox-clangdb.qbs",
        "blackbox/blackbox-java.qbs",
        "blackbox/blackbox-qt.qbs",
        "buildgraph/buildgraph.qbs",
        "cmdlineparser/cmdlineparser.qbs",
        "language/language.qbs",
        "tools/tools.qbs",
    ]
}
