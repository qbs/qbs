import qbs 1.0

Project {
    property bool enableUnitTests: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property path libInstallDir: qbs.targetOS.contains("windows") ? "bin" : "lib"
    property path libRPaths: (project.enableRPath && qbs.targetOS.contains("linux"))
                ? ["$ORIGIN/../lib"] : undefined
    property path resourcesInstallDir: ""

    references: [
        "src/src.qbs",
        "doc/doc.qbs",
        "share/share.qbs",
        "tests/auto/auto.qbs"
    ]
}
