Product {
    name: "two-default-property-values"
    type: "mymodule"
    Depends { name: "mymodule" }
    Depends { name: "myothermodule" }
    mymodule.direct: "dummy"
    Group {
        files: ["test.txt"]
        fileTags: ["txt"]
    }
}
