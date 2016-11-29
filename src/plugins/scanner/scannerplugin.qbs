import qbs 1.0

QbsProduct {
    Depends { name: "cpp" }
    Depends { name: "bundle"; condition: qbs.targetOS.contains("darwin") }
    Depends { name: "Qt.core" }
    Depends { name: "qbsbuildconfig" }
    type: Qt.core.staticBuild ? ["staticlibrary"] : ["dynamiclibrary"]
    cpp.defines: base.concat(type.contains("staticlibrary") ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.cxxLanguageVersion: "c++11"
    destinationDirectory: qbsbuildconfig.libDirName + "/qbs/plugins"
    Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.pluginsInstallDir + "/qbs/plugins"
    }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }
}
