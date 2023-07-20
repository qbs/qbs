//! [0]
// lib/lib.qbs

DynamicLibrary {
    name: "mylib"
    files: [
        "lib.c",
        "lib.h",
        "lib_global.h",
    ]
    version: "1.0.0"
    install: true

    Depends { name: "cpp" }
    cpp.defines: ["MYLIB_LIBRARY", "CRUCIAL_DEFINE"]
    cpp.sonamePrefix: qbs.targetOS.contains("darwin") ? "@rpath" : undefined

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
    }

    Depends { name: "bundle" }
    bundle.isBundle: false
}
//! [0]
