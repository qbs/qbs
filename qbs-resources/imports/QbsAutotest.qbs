import qbs
import qbs.FileInfo

QtApplication {
    type: ["application", "autotest"]
    consoleApplication: true
    property string testName
    name: "tst_" + testName
    Depends { name: "Qt.testlib" }
    Depends { name: "qbscore" }
    Depends { name: "qbsbuildconfig" }
    cpp.includePaths: [
        "../../../src",
        "../../../src/app/shared", // for the logger
    ]
    cpp.cxxLanguageVersion: "c++11"
    destinationDirectory: "bin"
    Group {
        name: "logging"
        prefix: FileInfo.joinPaths(product.sourceDirectory, "../../../src/app/shared/logging") + '/'
        files: [
            "coloredoutput.cpp",
            "coloredoutput.h",
            "consolelogger.cpp",
            "consolelogger.h"
        ]
    }
}
