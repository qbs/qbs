import qbs

Project {
    name: "Autotests"
    references: [
        "api/api.qbs",
        "blackbox/blackbox.qbs",
        "blackbox/blackbox-android.qbs",
        "blackbox/blackbox-apple.qbs",
        "blackbox/blackbox-baremetal.qbs",
        "blackbox/blackbox-clangdb.qbs",
        "blackbox/blackbox-examples.qbs",
        "blackbox/blackbox-java.qbs",
        "blackbox/blackbox-joblimits.qbs",
        "blackbox/blackbox-qt.qbs",
        "buildgraph/buildgraph.qbs",
        "cmdlineparser/cmdlineparser.qbs",
        "language/language.qbs",
        "tools/tools.qbs",
    ]
}
