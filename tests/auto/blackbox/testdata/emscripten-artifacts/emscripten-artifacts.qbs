CppApplication {
    name: "app"
    property bool dummy: { console.info("is emscripten: " + qbs.toolchain.includes("emscripten")); }
    files: "main.cpp"
}
