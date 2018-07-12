import qbs
import qbs.FileInfo

QbsProduct {
    Depends { name: "qbscore" }
    Depends { name: "cpp" }
    Depends { name: "qbsversion" }
    type: ["application", "qbsapplication"]
    version: qbsversion.version
    consoleApplication: true
    cpp.includePaths: [
        "../shared",    // for the logger
    ]
    Group {
        fileTagsFilter: product.type
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_app"] : [])
        qbs.install: true
        qbs.installDir: targetInstallDir
        qbs.installSourceBase: buildDirectory
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
