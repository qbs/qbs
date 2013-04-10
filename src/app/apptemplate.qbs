import qbs 1.0

Product {
    Depends { name: "qbscore" }
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }
    type: "application"
    consoleApplication: true
    destinationDirectory: "bin"
    cpp.includePaths: [
        "../shared",    // for the logger
    ]
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: "bin"
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

