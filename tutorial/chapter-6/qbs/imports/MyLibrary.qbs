DynamicLibrary {
    version: project.version
    install: true
    installDebugInformation: project.installDebugInformation

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
