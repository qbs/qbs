import qbs
import qbs.FileInfo

QbsProduct {
    Depends { name: "qbscore" }
    Depends { name: "cpp" }
    type: "application"
    consoleApplication: true
    destinationDirectory: "bin"
    cpp.includePaths: [
        "../shared",    // for the logger
    ]
    cpp.cxxLanguageVersion: "c++11"
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: qbsbuildconfig.appInstallDir
    }
    Group {
        name: "logging"
        prefix: FileInfo.joinPaths(product.sourceDirectory, "../shared/logging") + '/'
        files: [
            "coloredoutput.cpp",
            "coloredoutput.h",
            "consolelogger.cpp",
            "consolelogger.h"
        ]
    }
}
