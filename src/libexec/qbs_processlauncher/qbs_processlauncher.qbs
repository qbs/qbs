import qbs
import qbs.FileInfo

QbsProduct {
    type: "application"
    name: "qbs_processlauncher"
    consoleApplication: true
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory,
                                             qbsbuildconfig.libexecInstallDir)

    Depends { name: "Qt.network" }

    Properties {
        condition: qbs.targetOS.contains("macos") && qbsbuildconfig.enableRPath
        cpp.rpaths: ["@loader_path/../../" + qbsbuildconfig.libDirName]
    }

    cpp.cxxLanguageVersion: "c++11"
    cpp.includePaths: base.concat(pathToProtocolSources)

    files: [
        "launcherlogging.cpp",
        "launcherlogging.h",
        "launchersockethandler.cpp",
        "launchersockethandler.h",
        "processlauncher-main.cpp",
    ]

    property string pathToProtocolSources: sourceDirectory + "/../../lib/corelib/tools"
    Group {
        name: "protocol sources"
        prefix: pathToProtocolSources + '/'
        files: [
            "launcherpackets.cpp",
            "launcherpackets.h",
        ]
    }

    Group {
        fileTagsFilter: product.type
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_app"] : [])
        qbs.install: true
        qbs.installDir: qbsbuildconfig.libexecInstallDir
        qbs.installSourceBase: destinationDirectory
    }
}
