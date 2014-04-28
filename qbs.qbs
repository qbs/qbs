import qbs 1.0

Project {
    property bool enableUnitTests: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool withExamples: true
    property string libDirName: "lib"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property stringList libRPaths: {
        if (!project.enableRPath)
            return undefined;
        if (qbs.targetOS.contains("linux"))
            return ["$ORIGIN/../" + libDirName];
        if (qbs.targetOS.contains("osx"))
            return ["@loader_path/../" + libDirName]
    }
    property string resourcesInstallDir: ""
    property string pluginsInstallDir: libDirName

    references: [
        "doc/doc.qbs",
        "share/share.qbs",
        "src/src.qbs",
        "tests/auto/auto.qbs",
        "tests/fuzzy-test/fuzzy-test.qbs",
    ]

    SubProject {
        filePath: "examples/examples.qbs"
        condition: project.withExamples
    }
}
