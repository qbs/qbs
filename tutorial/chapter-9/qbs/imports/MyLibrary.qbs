// ![0]
Library {
    Depends { name: "cpp" }
    Depends { name: "mybuildconfig" }
    type: mybuildconfig.staticBuild ? "staticlibrary" : "dynamiclibrary"
    version: mybuildconfig.productVersion
    install: !mybuildconfig.staticBuild || mybuildconfig.installStaticLib
    installDir: mybuildconfig.libInstallDir

    readonly property string _nameUpper : name.replace(" ", "_").toUpperCase()
    property string libraryMacro: _nameUpper + "_LIBRARY"
    property string staticLibraryMacro: _nameUpper + "_STATIC_LIBRARY"
    cpp.defines: mybuildconfig.staticBuild ? [staticLibraryMacro] : [libraryMacro]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
        cpp.defines: exportingProduct.mybuildconfig.staticBuild
            ? [exportingProduct.staticLibraryMacro] : []
    }

    Depends { name: "bundle" }
    bundle.isBundle: false
}
// ![0]
