import qbs 1.0

Project {
    minimumQbsVersion: "1.4"
    qbsSearchPaths: ["qbs-resources"]
    property bool enableUnitTests: false
    property bool enableProjectFileUpdates: false
    property bool enableRPath: true
    property bool installApiHeaders: true
    property bool withExamples: false
    property string libDirName: "lib"
    property string appInstallDir: "bin"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property string libexecInstallDir: "libexec/qbs"
    property string relativeLibexecPath: "../" + libexecInstallDir
    property string relativePluginsPath: "../" + libDirName
    property string relativeSearchPath: ".."
    property stringList libRPaths: {
        if (!enableRPath)
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
        "tests/benchmarker/benchmarker.qbs",
    ]

    SubProject {
        filePath: "examples/examples.qbs"
        Properties {
            condition: parent.withExamples
        }
    }

    AutotestRunner {
        Depends { name: "Qt.core" }
        environment: {
            var env = base;
            if (qbs.hostOS.contains("windows") && qbs.targetOS.contains("windows")) {
                var path = "";
                for (var i = 0; i < env.length; ++i) {
                    if (env[i].startsWith("PATH=")) {
                        path = env[i].substring(5);
                        break;
                    }
                }
                path = Qt.core.binPath + ";" + path;
                var arrayElem = "PATH=" + path;
                if (i < env.length)
                    env[i] = arrayElem;
                else
                    env.push(arrayElem);
            }
            return env;
        }
    }
}
