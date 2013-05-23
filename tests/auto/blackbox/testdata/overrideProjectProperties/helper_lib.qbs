import qbs

DynamicLibrary {
    name: "helperLib"
    files: "helperlib.cpp"
    Depends { name: "cpp" }
}
