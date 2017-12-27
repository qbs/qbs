import qbs
import qbs.FileInfo
import qbs.Utilities

QbsProduct {
    Depends { name: "cpp" }
    version: qbsversion.version
    type: libType
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    cpp.defines: base.concat(visibilityType === "static" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
    Properties {
        condition: qbs.toolchain.contains("gcc")
        cpp.soVersion: version.replace(/\.\d+$/, '')
    }
    cpp.visibility: "minimal"
    property string visibilityType: staticBuild ? "static" : "dynamic"
    property string headerInstallPrefix: "/include/qbs"
    property bool hasExporter: Utilities.versionCompare(qbs.version, "1.12") >= 0
    property bool generatePkgConfigFile: qbsbuildconfig.generatePkgConfigFiles && hasExporter
    property bool generateQbsModule: install && qbsbuildconfig.generateQbsModules && hasExporter
    property bool staticBuild: Qt.core.staticBuild || qbsbuildconfig.staticBuild
    property stringList libType: [staticBuild ? "staticlibrary" : "dynamiclibrary"]
}
