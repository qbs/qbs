import qbs 1.0

Project {
    property bool enableUnitTests: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property path libInstallDir: qbs.targetOS.contains("windows") ? "bin" : "lib"
    property path libRPaths: {
        if (!project.enableRPath)
            return undefined;
        if (qbs.targetOS.contains("linux"))
            return ["$ORIGIN/../lib"];
        if (qbs.targetOS.contains("osx"))
            return ["@loader_path/../lib"]
    }
    property path resourcesInstallDir: ""

    references: [
        "src/src.qbs",
        "doc/doc.qbs",
        "share/share.qbs",
        "tests/auto/auto.qbs"
    ]
}
