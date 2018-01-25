import qbs
import qbs.FileInfo
import qbs.Utilities

QtApplication {
    type: ["application", "autotest"]
    consoleApplication: true
    property string testName
    name: "tst_" + testName
    Depends { name: "Qt.testlib" }
    Depends { name: "qbscore" }
    Depends { name: "qbsbuildconfig" }
    cpp.defines: [
        "QBS_TEST_SUITE_NAME=" + Utilities.cStringQuote(testName.toUpperCase().replace("-", "_"))
    ]
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
    cpp.rpaths: [FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix, qbsbuildconfig.libDirName)]
}
