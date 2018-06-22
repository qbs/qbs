Project {
    references: ["dep.qbs"]
    Product {
        name: "toplevel"
        type: ["rule-output"]
        Depends { name: "leaf" }
        Depends { name: "nonleaf" }
        Depends { name: "dep" }
        Group {
            files: ["dummy.txt"]
            fileTags: ["rule-input"]
        }
        // leaf.scalarProp: "product"
        // leaf.listProp: ["product"]
    }
}
