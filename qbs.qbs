import qbs 1.0

Project {
    minimumQbsVersion: "1.6"
    qbsSearchPaths: ["qbs-resources"]
    property bool withExamples: false
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

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

    Product {
        name: "version"
        files: ["VERSION"]
    }

    Product {
        name: "qmake project files for qbs"
        files: ["**/*.pr[io]"]
    }

    AutotestRunner {
        Depends { name: "Qt.core" }
        Depends { name: "qbs resources" }
        Depends { name: "qbs_cpp_scanner" }
        Depends { name: "qbs_qt_scanner" }
        arguments: project.autotestArguments
        wrapper: project.autotestWrapper
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
            if (qbs.hostOS.contains("darwin") && qbs.targetOS.contains("darwin")) {
                env.push("DYLD_FRAMEWORK_PATH=" + Qt.core.libPath);
                env.push("DYLD_LIBRARY_PATH=" + Qt.core.libPath);
            }
            return env;
        }
    }
}
