import qbs

QbsProduct {
    Depends { name: "cpp" }
    version: qbsversion.version
    type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    destinationDirectory: qbs.targetOS.contains("windows") ? "bin" : qbsbuildconfig.libDirName
    cpp.defines: base.concat(visibilityType === "static" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
    // ### Uncomment the following line in 1.8
    //cpp.soVersion: version.replace(/\.\d+$/, '')
    cpp.visibility: "minimal"
    cpp.cxxLanguageVersion: "c++11"
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

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
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
