// ![0]
Library {
    Depends { name: "cpp" }
    Depends { name: "config.myproject" }
    type: config.myproject.staticBuild ? "staticlibrary" : "dynamiclibrary"
    version: config.myproject.productVersion

    readonly property string _nameUpper : name.replace(" ", "_").toUpperCase()
    property string libraryMacro: _nameUpper + "_LIBRARY"
    property string staticLibraryMacro: _nameUpper + "_STATIC_LIBRARY"
    cpp.defines: config.myproject.staticBuild ? [staticLibraryMacro] : [libraryMacro]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
        cpp.defines: exportingProduct.config.myproject.staticBuild
            ? [exportingProduct.staticLibraryMacro] : []
    }

    Depends { name: "bundle" }
    bundle.isBundle: false
}
// ![0]
