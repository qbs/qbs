import qbs
import qbs.FileInfo

QbsProduct {
    property bool isForDarwin: qbs.targetOS.contains("darwin")
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: isForDarwin }
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    Depends { name: "qbscore"; condition: !Qt.core.staticBuild }
    type: (Qt.core.staticBuild ? ["staticlibrary"]
                               : [isForDarwin ? "loadablemodule" : "dynamiclibrary"])
        .concat(["qbsplugin"])
    Properties {
        condition: Qt.core.staticBuild
        cpp.defines: ["QBS_STATIC_LIB"]
    }
    cpp.cxxLanguageVersion: "c++11"
    cpp.includePaths: base.concat(["../../../lib/corelib"])
    cpp.visibility: "hidden"
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory,
                                             qbsbuildconfig.libDirName + "/qbs/plugins")
    Group {
        fileTagsFilter: [isForDarwin ? "loadablemodule" : "dynamiclibrary"]
            .concat(qbs.buildVariant === "debug"
        ? [isForDarwin ? "debuginfo_loadablemodule" : "debuginfo_dll"] : [])
        qbs.install: true
        qbs.installDir: qbsbuildconfig.pluginsInstallDir + "/qbs/plugins"
        qbs.installSourceBase: destinationDirectory
    }
    Properties {
        condition: isForDarwin
        bundle.isBundle: false
    }
}
