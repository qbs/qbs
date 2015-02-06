import qbs

Product {
    type: "mytype"
    Depends { name: "higherlevel" }
    Depends { name: "lowerlevel" }
    Group {
        files: ["dummy.txt"]
        fileTags: ["dummy-input"]
    }
}
