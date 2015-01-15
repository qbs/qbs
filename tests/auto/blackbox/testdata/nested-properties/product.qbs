import qbs

Product {
    type: "mytype"
    Depends { name: "higherlevel" }
    Group {
        files: ["dummy.txt"]
        fileTags: ["dummy-input"]
    }
}
