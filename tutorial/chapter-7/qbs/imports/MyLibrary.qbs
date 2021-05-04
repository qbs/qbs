//! [0]
// qbs/imports/MyLibrary.qbs
DynamicLibrary {
    version: project.version

    property pathList publicHeaders

    Depends { name: "config.myproject" }

    Group {
        condition: publicHeaders.length > 0
        name: "Public Headers"
        prefix: product.sourceDirectory + "/"
        files: publicHeaders
        qbs.install: config.myproject.installPublicHeaders
        qbs.installDir: installpaths.include
    }
    // ...
//! [0]
    Depends { name: "cpp" }
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
