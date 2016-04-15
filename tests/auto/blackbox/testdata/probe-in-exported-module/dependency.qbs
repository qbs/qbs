import qbs

Product {
    name: "dependency"
    Export {
        Depends { name: "mymodule" }
    }
}
