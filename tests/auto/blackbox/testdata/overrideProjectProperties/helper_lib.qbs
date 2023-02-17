DynamicLibrary {
    name: "helperLib"
    files: "helperlib.cpp"
    Depends { name: "cpp" }
    Properties {
        condition: qbs.targetOS.includes("darwin")
        bundle.isBundle: false
    }
}
