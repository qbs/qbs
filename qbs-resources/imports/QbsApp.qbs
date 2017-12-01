import qbs
import qbs.FileInfo

QbsProduct {
    Depends { name: "qbscore" }
    Depends { name: "cpp" }
    Depends { name: "qbsversion" }
    type: ["application", "qbsapplication"]
    version: qbsversion.version
    consoleApplication: true
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory, "bin")
    cpp.includePaths: [
        "../shared",    // for the logger
    ]
    cpp.cxxLanguageVersion: "c++11"
    Group {
        fileTagsFilter: product.type
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_app"] : [])
        qbs.install: true
        qbs.installSourceBase: destinationDirectory
        qbs.installDir: targetInstallDir
    }
    targetInstallDir: qbsbuildconfig.appInstallDir
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
