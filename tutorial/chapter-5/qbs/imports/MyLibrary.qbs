DynamicLibrary {
    version: "1.0.0"
    install: true

    Depends { name: 'cpp' }
    property string libraryMacro: name.replace(" ", "_").toUpperCase() + "_LIBRARY"
    cpp.defines: [libraryMacro]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
    }

    Depends { name: 'bundle' }
    bundle.isBundle: false
}
