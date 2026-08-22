CppLibrary {
    name: "foo"
    version: "3.0.1"
    files: "foo.c"
    publicHeaders: ["foo.h", "foo_global.h"]

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
