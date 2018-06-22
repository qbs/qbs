DynamicLibrary {
    name: "mylib"
    property bool useVersion
    version: useVersion ? "1.2.3" : undefined
    Depends { name: "cpp" }
    files: ["lib.cpp"]
}
