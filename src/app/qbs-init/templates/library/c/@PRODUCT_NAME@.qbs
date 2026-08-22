CppLibrary {
    name: "@PRODUCT_NAME@"
    version: "@PRODUCT_VERSION@"
    files: "@PRODUCT_NAME@.c"
    publicHeaders: ["@PRODUCT_NAME@.h", "@PRODUCT_NAME@_global.h"]

    readonly property string _nameUpper: name.replace(" ", "_").toUpperCase()
    property string libraryMacro: _nameUpper + "_LIBRARY"
    property string staticLibraryMacro: _nameUpper + "_STATIC_LIBRARY"
    cpp.defines: config.build.libraryType === "static" ? [staticLibraryMacro] : [libraryMacro]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
        cpp.defines: exportingProduct.config.build.libraryType === "static"
            ? [exportingProduct.staticLibraryMacro] : []
    }

    Depends { name: "bundle" }
    bundle.isBundle: false
}
