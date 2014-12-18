import qbs

DynamicLibrary {
    name: "helperLib"
    files: "helperlib.cpp"
    Depends { name: "cpp" }
    bundle.isBundle: false
}
