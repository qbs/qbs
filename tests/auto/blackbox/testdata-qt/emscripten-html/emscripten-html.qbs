Product {
    name: "app"
    type: generateHtml ? ["application", "qthtml"] : ["application"]

    Depends { name: "Qt.core" }

    files: "main.cpp"
    property bool dummy: { console.info("is emscripten: " + qbs.toolchain.includes("emscripten")); }
    property bool generateHtml: false
    Group {
        fileTagsFilter: ["qthtml"]
        qbs.install: true
    }
}

