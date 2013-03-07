import qbs 1.0

Product {
    type: "staticlibrary"
    name: "lol"
    files: [ "lib.cpp" ]
    cpp.defines: ['CRUCIAL_DEFINE']
    Depends { name: 'cpp' }
}

