import qbs
import qbs.FileInfo

QbsProduct {
    property bool isForDarwin: qbs.targetOS.contains("darwin")
    property bool staticBuild: Qt.core.staticBuild || qbsbuildconfig.staticBuild
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: isForDarwin }
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    Depends { name: "qbscore"; condition: !staticBuild }
    type: (staticBuild ? ["staticlibrary"] : [isForDarwin ? "loadablemodule" : "dynamiclibrary"])
        .concat(["qbsplugin"])
    Properties {
        condition: staticBuild
        cpp.defines: ["QBS_STATIC_LIB"]
    }
    cpp.includePaths: base.concat(["../../../lib/corelib"])
    cpp.visibility: "minimal"
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory,
                                             qbsbuildconfig.libDirName + "/qbs/plugins")
    Group {
        fileTagsFilter: [isForDarwin ? "loadablemodule" : "dynamiclibrary"]
            .concat(qbs.buildVariant === "debug"
        ? [isForDarwin ? "debuginfo_loadablemodule" : "debuginfo_dll"] : [])
        qbs.install: true
        qbs.installSourceBase: destinationDirectory
        qbs.installDir: targetInstallDir
    }
    targetInstallDir: qbsbuildconfig.pluginsInstallDir
    Properties {
        condition: isForDarwin
        bundle.isBundle: false
    }
}
