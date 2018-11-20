import qbs
import qbs.FileInfo

Project {
    references: [
        "auto/auto.qbs",
        "benchmarker/benchmarker.qbs",
        "fuzzy-test/fuzzy-test.qbs",
    ]

    AutotestRunner {
        Depends { name: "Qt.core" }
        Depends { name: "qbs resources" }
        Depends { name: "qbs_cpp_scanner" }
        Depends { name: "qbs_qt_scanner" }
        arguments: project.autotestArguments
        wrapper: project.autotestWrapper
        environment: {
            var env = base;
            env.push("QBS_INSTALL_DIR=" + FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix));
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
