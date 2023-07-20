//! [0]
// lib/lib.qbs

MyLibrary {
    name: "mylib"
    files: [
        "lib.c",
        "lib.h",
        "lib_global.h",
    ]
    cpp.defines: base.concat(["CRUCIAL_DEFINE"])
}
//! [0]
