import qbs
import qbs.FileInfo

QbsProduct {
    type: "application"
    name: "qbs_processlauncher"
    consoleApplication: true

    Depends { name: "Qt.network" }

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
        qbs.installDir: targetInstallDir
        qbs.installSourceBase: buildDirectory
    }
    targetInstallDir: qbsbuildconfig.libexecInstallDir
}
