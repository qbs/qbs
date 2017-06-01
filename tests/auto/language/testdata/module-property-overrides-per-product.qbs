import qbs

Project {
    Product {
        Depends { name: "dummy" }
        name: "a"
    }
    Product {
        Depends { name: "dummy" }
        name: "b"
    }
    Product {
        Depends { name: "dummy" }
        name: "c"
    }
}
