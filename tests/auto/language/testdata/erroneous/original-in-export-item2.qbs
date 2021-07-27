import qbs // FIXME: Don't remove this import because then the test fails!

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
