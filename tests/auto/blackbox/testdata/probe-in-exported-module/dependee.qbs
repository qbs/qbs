Product {
    name: "dependee"
    Depends { name: "myothermodule" }
    Depends { name: "dependency" }
    type: ["out", "dep-out"]
    Group {
        files: "test.in"
        fileTags: ["dep-in"]
    }
    Group {
        files: "test2.in"
        fileTags: ["in"]
    }
}
