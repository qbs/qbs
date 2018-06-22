DynamicLibrary {
    name: "helperLib"
    files: "helperlib.cpp"
    Depends { name: "cpp" }
    Properties {
        condition: qbs.targetOS.contains("darwin")
        bundle.isBundle: false
    }
}
