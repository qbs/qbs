import qbs
import qbs.FileInfo

QbsProduct {
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: qbs.targetOS.contains("darwin") }
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    Depends { name: "qbscore"; condition: !Qt.core.staticBuild }
    type: (Qt.core.staticBuild ? ["staticlibrary"] : ["dynamiclibrary"]).concat(["qbsplugin"])
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
        fileTagsFilter: ["dynamiclibrary"]
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_dll"] : [])
        qbs.install: true
        qbs.installDir: qbsbuildconfig.pluginsInstallDir + "/qbs/plugins"
        qbs.installSourceBase: destinationDirectory
    }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }
}
