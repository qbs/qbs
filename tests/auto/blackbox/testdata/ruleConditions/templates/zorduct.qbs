import qbs 1.0

Product {
    type: ["application", "zort"]
    Depends { name: "cpp" }
    Depends { name: "narfzort" }
    files: [
        "main.cpp",
        "foo.narf"
    ]
}
