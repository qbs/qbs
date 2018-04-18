import qbs
import qbs.FileInfo

QbsProduct {
    Depends { name: "cpp" }
    version: qbsversion.version
    type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory,
        qbs.targetOS.contains("windows") ? "bin" : qbsbuildconfig.libDirName)
    cpp.defines: base.concat(visibilityType === "static" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined
    Properties {
        condition: qbs.toolchain.contains("gcc")
        cpp.soVersion: version.replace(/\.\d+$/, '')
    }
    cpp.visibility: "minimal"
    cpp.cxxLanguageVersion: "c++11"
    property string visibilityType: Qt.core.staticBuild ? "static" : "dynamic"
    property string headerInstallPrefix: "/include/qbs"
    Group {
        fileTagsFilter: product.type.concat("dynamiclibrary_symlink")
            .concat(qbs.buildVariant === "debug" ? ["debuginfo_dll"] : [])
        qbs.install: install
        qbs.installSourceBase: destinationDirectory
        qbs.installDir: targetInstallDir
    }
    targetInstallDir: qbsbuildconfig.libInstallDir
    Group {
        fileTagsFilter: ["dynamiclibrary_import"]
        qbs.install: install
        qbs.installDir: qbsbuildconfig.importLibInstallDir
    }

    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
        cpp.linkerFlags: ["-compatibility_version", cpp.soVersion]
    }

    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }

        cpp.includePaths: [product.sourceDirectory]
        cpp.defines: product.visibilityType === "static" ? ["QBS_STATIC_LIB"] : []
    }
}
