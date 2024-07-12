Project {
    Product {
        name: "a"
        Export {
            Properties {
                x.y.z: original
            }
        }
    }
    Product {
        name: "b"
        Depends { name: "a" }
    }
}
