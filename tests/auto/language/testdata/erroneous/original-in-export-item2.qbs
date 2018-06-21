import qbs

Project {
    Product {
        name: "a"
        Export {
            x.y.z: original
        }
    }
    Product {
        name: "b"
        Depends { name: "a" }
    }
}
