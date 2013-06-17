import qbs

Project {
    name: "Autotests"
    references: [
        "blackbox/blackbox.qbs",
        "cmdlineparser/cmdlineparser.qbs"
    ].concat(unitTests)

    property pathList unitTests: enableUnitTests ? [
        "buildgraph/buildgraph.qbs",
        "language/language.qbs",
        "tools/tools.qbs"
    ] : []
}
