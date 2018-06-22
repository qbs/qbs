Product {
    name: "dep"
    Export {
        Depends { name: "leaf" }
        Depends { name: "nonleaf" }
        // leaf.scalarProp: "export"
        // leaf.listProp: ["export"]
    }
}
