import qbs 1.0

Product {
    type: "staticlibrary"
    name: "app-and-lib-lib"
    files: [ "lib.cpp" ]
    cpp.defines: ['CRUCIAL_DEFINE']
    Depends { name: 'cpp' }
}

