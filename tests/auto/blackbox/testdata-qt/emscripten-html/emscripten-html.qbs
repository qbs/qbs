QtApplication {
    name: "app"
    files: "main.cpp"
    property bool dummy: { console.info("is emscripten: " + qbs.toolchain.includes("emscripten")); }
    Group {
        fileTagsFilter: ["qthtml"]
        qbs.install: true
    }
}

