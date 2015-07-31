import qbs
import QbsFunctions

QbsProduct {
    Depends { name: "cpp" }
    Depends { name: "bundle" }
    version: QbsFunctions.qbsVersion()
    type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    destinationDirectory: qbs.targetOS.contains("windows") ? "bin" : project.libDirName
    cpp.defines: base.concat(type == "staticlibrary" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    cpp.installNamePrefix: "@rpath"
    cpp.visibility: "minimal"
    cpp.cxxLanguageVersion: "c++11"
    bundle.isBundle: false
    property string headerInstallPrefix: "/include/qbs"
    Group {
        fileTagsFilter: product.type.concat("dynamiclibrary_symlink")
        qbs.install: true
        qbs.installDir: project.libInstallDir
    }
    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }
        cpp.rpaths: project.libRPaths
        cpp.includePaths: [product.sourceDirectory]
        cpp.defines: product.type === "staticlibrary" ? ["QBS_STATIC_LIB"] : []
    }
}
