Project {
    Product {
        name: "p1"
        Export {
            something.other: "x"
        }
    }
    Product {
        Depends { name: "p1" }
    }
}
