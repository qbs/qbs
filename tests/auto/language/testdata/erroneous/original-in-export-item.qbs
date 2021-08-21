import qbs // FIXME: Don't remove this import because then the test fails!

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
