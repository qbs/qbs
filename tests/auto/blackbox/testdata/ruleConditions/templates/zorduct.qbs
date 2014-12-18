import qbs 1.0

Product {
    type: ["application", "zort"]
    consoleApplication: true
    Depends { name: "cpp" }
    Depends { name: "narfzort" }
    files: [
        "main.cpp",
        "foo.narf"
    ]
}
