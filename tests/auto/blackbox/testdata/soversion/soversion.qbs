DynamicLibrary {
    name: "mylib"
    property bool useVersion
    property bool dummy: { console.info("is emscripten: " + qbs.toolchain.includes("emscripten")); }
    version: useVersion ? "1.2.3" : undefined
    Depends { name: "cpp" }
    files: ["lib.cpp"]
}
