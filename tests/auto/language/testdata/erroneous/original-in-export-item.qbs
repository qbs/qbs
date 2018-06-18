import qbs

Project {
    Product {
        name: "a"
        Export {
            property string p: original
        }
    }
    Product {
        name: "b"
        Depends { name: "a" }
    }
}
