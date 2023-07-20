DynamicLibrary {
    version: project.version
    install: true
    installDebugInformation: project.installDebugInformation
//! [0]
// qbs/imports/MyLibrary.qbs
    // ...
    Depends { name: "mybuildconfig" }
    installDir: mybuildconfig.libInstallDir

    Depends { name: "cpp" }
    property string libraryMacro: name.replace(" ", "_").toUpperCase() + "_LIBRARY"
    cpp.defines: [libraryMacro]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
    // ...
//! [0]
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
    }

    Depends { name: 'bundle' }
    bundle.isBundle: false
}
