import qbs

QtApplication {
    type: ["application", "autotest"]
    property string testName
    name: "tst_" + testName
    Depends { name: "Qt.test" }
    Depends { name: "qbscore" }
    cpp.includePaths: "../../../src"
    destinationDirectory: "bin"
    Group {
        name: "logging"
        prefix: "../../../src/app/shared/logging/"
        files: [
            "coloredoutput.cpp",
            "coloredoutput.h",
            "consolelogger.cpp",
            "consolelogger.h"
        ]
    }
}
