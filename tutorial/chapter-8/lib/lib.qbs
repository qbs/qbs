MyLibrary {
    name: "mylib"
    files: [
        "lib.c",
        "lib.h",
    ]
    Depends { name: 'cpp' }
    cpp.defines: ['CRUCIAL_DEFINE']
}

