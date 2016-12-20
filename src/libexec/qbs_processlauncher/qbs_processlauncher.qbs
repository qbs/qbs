import qbs
import qbs.FileInfo

QbsProduct {
    type: "application"
    name: "qbs_processlauncher"
    consoleApplication: true
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory,
                                             qbsbuildconfig.libexecInstallDir)

    Depends { name: "Qt.network" }

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
        qbs.install: true
        qbs.installDir: qbsbuildconfig.libexecInstallDir
    }
}
