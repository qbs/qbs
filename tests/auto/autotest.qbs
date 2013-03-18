import qbs

CppApplication {
    type: "application"
    name: "tst_" + testName
    Depends { name: "Qt.test" }
    Depends { name: "qbscore" }
    cpp.includePaths: path + "/../../src"
    destinationDirectory: "bin"
    Group {
        name: "logging"
        prefix: path + "/../../src/app/shared/logging/"
        files: [
            "coloredoutput.cpp",
            "coloredoutput.h",
            "consolelogger.cpp",
            "consolelogger.h"
        ]
    }
}
