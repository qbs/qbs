import qbs
import QbsFunctions

QbsProduct {
    Depends { name: "cpp" }
    Depends { name: "bundle" }
    version: QbsFunctions.qbsVersion()
    type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    destinationDirectory: qbs.targetOS.contains("windows") ? "bin" : qbsbuildconfig.libDirName
    cpp.defines: base.concat(visibilityType === "static" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
    // ### Uncomment the following line in 1.8
    //cpp.soVersion: version.replace(/\.\d+$/, '')
    cpp.visibility: "minimal"
    cpp.cxxLanguageVersion: "c++11"
    bundle.isBundle: false
    property bool visibilityType: Qt.core.staticBuild ? "static" : "dynamic"
    property string headerInstallPrefix: "/include/qbs"
    Group {
        fileTagsFilter: product.type.concat("dynamiclibrary_symlink")
        qbs.install: install
        qbs.installDir: qbsbuildconfig.libInstallDir
    }
    Group {
        fileTagsFilter: ["dynamiclibrary_import"]
        qbs.install: install
        qbs.installDir: qbsbuildconfig.importLibInstallDir
    }

    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }
        Depends { name: "qbsbuildconfig" }

        cpp.rpaths: qbsbuildconfig.libRPaths
        cpp.includePaths: [product.sourceDirectory]
        cpp.defines: product.visibilityType === "static" ? ["QBS_STATIC_LIB"] : []
    }
}
