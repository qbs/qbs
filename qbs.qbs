import qbs 1.0

Project {
    property bool enableUnitTests: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool withExamples: true
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : "lib"
    property string libRPaths: {
        if (!project.enableRPath)
            return undefined;
        if (qbs.targetOS.contains("linux"))
            return ["$ORIGIN/../lib"];
        if (qbs.targetOS.contains("osx"))
            return ["@loader_path/../lib"]
    }
    property string resourcesInstallDir: ""

    references: [
        "doc/doc.qbs",
        "share/share.qbs",
        "src/src.qbs",
        "tests/auto/auto.qbs"
    ]

    SubProject {
        filePath: "examples/examples.qbs"
        condition: project.withExamples
    }
}
