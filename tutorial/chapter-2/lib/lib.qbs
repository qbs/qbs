//! [0]
StaticLibrary {
    name: "mylib"
    files: [
        "lib.c",
        "lib.h",
    ]
    version: "1.0.0"

    //! [1]
    cpp.defines: ['CRUCIAL_DEFINE']
    //! [1]

    //! [2]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [exportingProduct.sourceDirectory]
    }
    //! [2]

    //! [3]
    Depends { name: 'bundle' }
    bundle.isBundle: false
    //! [3]
}
//! [0]
