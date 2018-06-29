import qbs

Project {
    Product {
        name: "a"
        Export {
            Properties {
                condition: true
                x.y.z: original
            }
        }
    }
    Product {
        name: "b"
        Depends { name: "a" }
    }
}
