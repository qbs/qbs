Product {
    type: ["application", "zort"]
    consoleApplication: true
    property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
    Depends { name: "cpp" }
    Depends { name: "narfzort" }
    files: [
        "main.cpp",
        "foo.narf"
    ]
}
