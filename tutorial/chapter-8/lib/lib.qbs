MyLibrary {
    name: "mylib"
    files: [ "lib.c" ]
    publicHeaders: [ "lib.h" ]
    Depends { name: 'cpp' }
    cpp.defines: ['CRUCIAL_DEFINE']
}

