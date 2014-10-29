import qbs

QbsProduct {
    Depends { name: "qbscore" }
    Depends { name: "cpp" }
    type: "application"
    consoleApplication: true
    destinationDirectory: "bin"
    cpp.includePaths: [
        "../shared",    // for the logger
    ]
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: project.appInstallDir
    }
    Group {
        name: "logging"
        prefix: "../shared/logging/"
        files: [
            "coloredoutput.cpp",
            "coloredoutput.h",
            "consolelogger.cpp",
            "consolelogger.h"
        ]
    }
}
